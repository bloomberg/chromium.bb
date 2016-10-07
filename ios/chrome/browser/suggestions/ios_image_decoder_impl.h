// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_
#define IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/image_fetcher/image_decoder.h"

namespace base {
class TaskRunner;
}

namespace suggestions {

// Factory for iOS specific implementation of image_fetcher::ImageDecoder.
std::unique_ptr<image_fetcher::ImageDecoder> CreateIOSImageDecoder(
    scoped_refptr<base::TaskRunner> task_runner);

}  // namespace suggestions

#endif  // IOS_CHROME_BROWSER_SUGGESTIONS_IOS_IMAGE_DECODER_IMPL_H_
