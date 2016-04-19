// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_decoder_impl.h"

#include <string.h>

#include <utility>

#include "base/logging.h"
#include "content/public/child/image_decoder_utils.h"
#include "ipc/ipc_channel.h"
#include "skia/ext/image_operations.h"
#include "skia/public/type_converters.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_CHROMEOS)
#include "ui/gfx/chromeos/codec/jpeg_codec_robust_slow.h"
#include "ui/gfx/codec/png_codec.h"
#endif

namespace {
int64_t kMaxMessageSize = IPC::Channel::kMaximumMessageSize;
int64_t kPadding = 64;
}

ImageDecoderImpl::ImageDecoderImpl(int64_t max_message_size)
    : max_message_size_(max_message_size), binding_(this) {
}

ImageDecoderImpl::ImageDecoderImpl(
    mojo::InterfaceRequest<mojom::ImageDecoder> request)
    : max_message_size_(kMaxMessageSize), binding_(this, std::move(request)) {
}

ImageDecoderImpl::~ImageDecoderImpl() {
}

void ImageDecoderImpl::DecodeImage(
    mojo::Array<uint8_t> encoded_data,
    mojom::ImageCodec codec,
    bool shrink_to_fit,
    const mojo::Callback<void(skia::mojom::BitmapPtr)>& callback) {
  if (encoded_data.size() == 0) {
    callback.Run(nullptr);
    return;
  }

  SkBitmap decoded_image;
#if defined(OS_CHROMEOS)
  if (codec == mojom::ImageCodec::ROBUST_JPEG) {
    // Our robust jpeg decoding is using IJG libjpeg.
    if (encoded_data.size()) {
      std::unique_ptr<SkBitmap> decoded_jpeg(gfx::JPEGCodecRobustSlow::Decode(
          encoded_data.storage().data(), encoded_data.size()));
      if (decoded_jpeg.get() && !decoded_jpeg->empty())
        decoded_image = *decoded_jpeg;
    }
  } else if (codec == mojom::ImageCodec::ROBUST_PNG) {
    // Our robust PNG decoding is using libpng.
    if (encoded_data.size()) {
      SkBitmap decoded_png;
      if (gfx::PNGCodec::Decode(encoded_data.storage().data(),
                                encoded_data.size(), &decoded_png)) {
        decoded_image = decoded_png;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)
  if (codec == mojom::ImageCodec::DEFAULT) {
    decoded_image = content::DecodeImage(encoded_data.storage().data(),
                                         gfx::Size(), encoded_data.size());
  }

  if (!decoded_image.isNull()) {
    skia::mojom::BitmapPtr dummy_image = skia::mojom::Bitmap::New();
    int64_t struct_size =
        skia::mojom::GetSerializedSize_(dummy_image, nullptr) + kPadding;
    int64_t image_size = decoded_image.computeSize64();
    int halves = 0;
    while (struct_size + (image_size >> 2 * halves) > max_message_size_)
      halves++;
    if (halves) {
      if (shrink_to_fit) {
        // If decoded image is too large for IPC message, shrink it by halves.
        // This prevents quality loss, and should never overshrink on displays
        // smaller than 3600x2400.
        // TODO (Issue 416916): Instead of shrinking, return via shared memory
        decoded_image = skia::ImageOperations::Resize(
            decoded_image, skia::ImageOperations::RESIZE_LANCZOS3,
            decoded_image.width() >> halves, decoded_image.height() >> halves);
      } else {
        decoded_image.reset();
      }
    }
  }

  if (decoded_image.isNull())
    callback.Run(nullptr);
  else
    callback.Run(skia::mojom::Bitmap::From(decoded_image));
}
