/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/picture.hpp>

#include "SkBitmap.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkPicture.h"
#include "SkSurface.h"
#include "SkCanvas.h"
#include "SkPictureRecorder.h"

#include "opaque.hpp"
#include <stdexcept>

namespace cycfi::artist
{
   picture::picture(point size)
    : _host{ new artist::host_picture(size) }
   {}

   picture::picture(std::string_view path_)
    : _host{ new artist::host_picture(SkBitmap{}) }
   {
      std::string path{ path_ };
      auto fail = [&path]()
      {
         throw std::runtime_error{ "Error: Failed to load file: " + path };
      };

      sk_sp<SkData> data{ SkData::MakeFromFileName(path.c_str()) };
      std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(data);
      if (!codec)
         fail();
      SkImageInfo info = codec->getInfo().makeColorType(kN32_SkColorType);

      auto& bitmap = std::get<SkBitmap>(*_host);
      if (!bitmap.tryAllocPixels(info))
         fail();

      if (codec->getPixels(info, bitmap.getPixels(), bitmap.rowBytes()) != SkCodec::kSuccess)
         fail();
   }

   picture::~picture()
   {
      delete _host;
   }

   host_picture_ptr picture::host_picture() const
   {
      return _host;
   }

   extent picture::size() const
   {
      auto get_size =
         [](auto const& that) -> extent
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, extent>)
            {
               return that;
            }
            if constexpr(std::is_same_v<T, sk_sp<SkPicture>>)
            {
               auto r = that->cullRect();
               return extent{ r.width(), r.height() };
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               return extent{ float(that.width()), float(that.height()) };
            }
            return {};
         };

      return std::visit(get_size, *_host);
   }

   void picture::save_png(std::string_view path_) const
   {
      std::string path{ path_ };
      auto fail = [&path]()
      {
         throw std::runtime_error{ "Error: Failed to save file: " + path };
      };

      auto size_ = size();
      sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(size_.x, size_.y);
      SkCanvas* sk_canvas = surface->getCanvas();

      auto draw_picture =
         [&](auto const& that)
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, extent>)
            {
            }
            if constexpr(std::is_same_v<T, sk_sp<SkPicture>>)
            {
               sk_canvas->drawPicture(that);
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               sk_canvas->drawBitmap(that, 0, 0);
            }
         };

      std::visit(draw_picture, *_host);

      // Make a PNG encoded image using the canvas
      sk_sp<SkImage> image(surface->makeImageSnapshot());
      if (!image)
         fail();

      sk_sp<SkData> png(image->encodeToData());
      if (!png)
         fail();

      // write the data to the file specified by filePath
      SkFILEWStream out(path.c_str());
      out.write(png->data(), png->size());
   }

   uint32_t* picture::pixels()
   {
      return nullptr;
   }

   uint32_t const* picture::pixels() const
   {
      return nullptr;
   }

   extent picture::bitmap_size() const
   {
      return {};
   }

   struct picture_context::state
   {
      SkPictureRecorder recorder;
      SkCanvas* recording_canvas;
   };

   picture_context::picture_context(picture& pict)
    : _picture{ pict }
    , _state{ new picture_context::state{} }
   {
      auto size = pict.size();
      _state->recording_canvas = _state->recorder.beginRecording(size.x, size.y);
   }

   picture_context::~picture_context()
   {
      *(_picture.host_picture()) = _state->recorder.finishRecordingAsPicture();
      delete _state;
   }

   host_context_ptr picture_context::context() const
   {
      return host_context_ptr(_state->recording_canvas);
   }
}
