// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SIMPLE_PNG_ENCODER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SIMPLE_PNG_ENCODER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
#include "base/synchronization/cancellation_flag.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chromeos {

typedef base::Callback<void(
    scoped_refptr<base::RefCountedBytes>)> EncoderCallback;

// Operation class that encodes existing in-memory image as PNG.
// It uses NO-COMPRESSION to save time.
class SimplePngEncoder
    : public base::RefCountedThreadSafe<SimplePngEncoder> {
 public:
  SimplePngEncoder(scoped_refptr<base::RefCountedBytes> data,
                   SkBitmap image,
                   base::Closure cancel_callback);

  void Run(EncoderCallback callback);

  void EncodeWallpaper();
  void Cancel();

 private:
  friend class base::RefCountedThreadSafe<SimplePngEncoder>;

  ~SimplePngEncoder();

  base::CancellationFlag cancel_flag_;

  // Buffer to store encoded image.
  scoped_refptr<base::RefCountedBytes> data_;
  // Original image to encode.
  SkBitmap image_;
  base::Closure cancel_callback_;

  DISALLOW_COPY_AND_ASSIGN(SimplePngEncoder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_SIMPLE_PNG_ENCODER_H_
