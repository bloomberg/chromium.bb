// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/ios_image_decoder_impl.h"

#include <UIKit/UIKit.h>

#include "base/callback.h"
#include "ui/gfx/image/image.h"

namespace suggestions {

IOSImageDecoderImpl::IOSImageDecoderImpl() {}

IOSImageDecoderImpl::~IOSImageDecoderImpl() {}

void IOSImageDecoderImpl::DecodeImage(
    const std::string& image_data,
    const image_fetcher::ImageDecodedCallback& callback) {
  // Convert the |image_data| std::string to a NSData buffer.
  NSData* data =
      [NSData dataWithBytesNoCopy:const_cast<char*>(image_data.c_str())
                           length:image_data.length()
                     freeWhenDone:NO];

  // Decode the Image using UIImage.
  if (data) {
    // Most likely always returns 1x images.
    UIImage* ui_image = [UIImage imageWithData:data scale:1];
    if (ui_image) {
      gfx::Image gfx_image(ui_image);
      callback.Run(gfx_image);
      return;
    }
  }
  gfx::Image empty_image;
  callback.Run(empty_image);
}

}  // namespace suggestions
