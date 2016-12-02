// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
#define IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/image_fetcher/image_fetcher.h"

class GURL;

namespace base {
class SequencedWorkerPool;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcherDelegate;
}

namespace net {
class URLRequestContextGetter;
}

namespace web {
class ImageDataFetcher;
}

namespace suggestions {

// A class used to fetch server images asynchronously.
class ImageFetcherImpl : public image_fetcher::ImageFetcher {
 public:
  ImageFetcherImpl(net::URLRequestContextGetter* url_request_context,
                   base::SequencedWorkerPool* blocking_pool);
  ~ImageFetcherImpl() override;

  void SetImageFetcherDelegate(
      image_fetcher::ImageFetcherDelegate* delegate) override;

  void SetDataUseServiceName(DataUseServiceName data_use_service_name) override;

  void StartOrQueueNetworkRequest(
      const std::string& id,
      const GURL& image_url,
      base::Callback<void(const std::string&, const gfx::Image&)> callback)
      override;

 private:
  std::unique_ptr<web::ImageDataFetcher> image_fetcher_;

  image_fetcher::ImageFetcherDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace suggestions

#endif  // IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
