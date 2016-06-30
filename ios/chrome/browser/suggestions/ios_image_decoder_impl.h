// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_
#define IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_

#include "base/macros.h"
#include "components/image_fetcher/image_decoder.h"

namespace suggestions {

class IOSImageDecoderImpl : public image_fetcher::ImageDecoder {
 public:
  IOSImageDecoderImpl();
  ~IOSImageDecoderImpl() override;

  void DecodeImage(
      const std::string& image_data,
      const image_fetcher::ImageDecodedCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSImageDecoderImpl);
};

}  // namespace suggestions

#endif  // IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_
