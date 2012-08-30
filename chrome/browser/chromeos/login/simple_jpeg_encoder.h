// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_JPEG_ENCODER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_JPEG_ENCODER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
#include "base/synchronization/cancellation_flag.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chromeos {

// Operation class that encodes existing in-memory image as JPEG.
class SimpleJpegEncoder
    : public base::RefCountedThreadSafe<SimpleJpegEncoder> {
 public:
  // This callback is called after encoding to JPEG completes.
  typedef base::Callback<void(
      scoped_refptr<base::RefCountedBytes>)> EncoderCallback;

  SimpleJpegEncoder(scoped_refptr<base::RefCountedBytes> data,
                    SkBitmap image);

  // Starts encoding task on worker pool.
  void Run(EncoderCallback callback);

  // Cancels encoding task when possible.
  void Cancel();

 private:
  friend class base::RefCountedThreadSafe<SimpleJpegEncoder>;

  ~SimpleJpegEncoder();

  // Encodes a SkBitmap with JPEG codec on worker pool.
  void EncodeWallpaper();

  base::CancellationFlag cancel_flag_;

  // Buffer to store encoded image.
  scoped_refptr<base::RefCountedBytes> data_;
  // Original image to encode.
  SkBitmap image_;

  DISALLOW_COPY_AND_ASSIGN(SimpleJpegEncoder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIMPLE_JPEG_ENCODER_H_
