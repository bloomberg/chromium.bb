// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/simple_png_encoder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

extern "C" {
#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif
}

namespace chromeos {

SimplePngEncoder::SimplePngEncoder(
    scoped_refptr<base::RefCountedBytes> data,
    SkBitmap image,
    base::Closure cancel_callback)
    : data_(data),
      image_(image),
      cancel_callback_(cancel_callback) {
}

void SimplePngEncoder::Run(EncoderCallback callback) {
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&SimplePngEncoder::EncodeWallpaper, this),
      base::Bind(callback, data_),
      true /* task_is_slow */);
}

void SimplePngEncoder::EncodeWallpaper() {
  if (cancel_flag_.IsSet())
    return;
  SkAutoLockPixels lock_input(image_);
  // Avoid compression to make things faster.
  gfx::PNGCodec::EncodeWithCompressionLevel(
      reinterpret_cast<unsigned char*>(image_.getAddr32(0, 0)),
      gfx::PNGCodec::FORMAT_SkBitmap,
      gfx::Size(image_.width(), image_.height()),
      image_.width() * image_.bytesPerPixel(),
      false,
      std::vector<gfx::PNGCodec::Comment>(),
      Z_NO_COMPRESSION,
      &data_->data());
}

void SimplePngEncoder::Cancel() {
  cancel_flag_.Set();
  cancel_callback_.Run();
}

SimplePngEncoder::~SimplePngEncoder() {}

}  // namespace chromeos
