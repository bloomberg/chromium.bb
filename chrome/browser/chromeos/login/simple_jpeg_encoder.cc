// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/simple_jpeg_encoder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

namespace chromeos {

namespace {
const int kCustomWallpaperQuality = 90;
}

SimpleJpegEncoder::SimpleJpegEncoder(
    scoped_refptr<base::RefCountedBytes> data,
    SkBitmap image)
    : data_(data),
      image_(image) {
}

void SimpleJpegEncoder::Run(EncoderCallback callback) {
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&SimpleJpegEncoder::EncodeWallpaper, this),
      base::Bind(callback, data_),
      true /* task_is_slow */);
}

void SimpleJpegEncoder::EncodeWallpaper() {
  if (cancel_flag_.IsSet())
    return;
  SkAutoLockPixels lock_input(image_);
  gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(image_.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap,
      image_.width(),
      image_.height(),
      image_.width() * image_.bytesPerPixel(),
      kCustomWallpaperQuality, &data_->data());
}

void SimpleJpegEncoder::Cancel() {
  cancel_flag_.Set();
}

SimpleJpegEncoder::~SimpleJpegEncoder() {
}

}  // namespace chromeos
