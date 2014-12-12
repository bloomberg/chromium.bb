// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
#define IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "components/suggestions/image_fetcher.h"

class GURL;
class SkBitmap;

namespace image_fetcher {
class ImageFetcher;
}

namespace base {
class SequencedWorkerPool;
}

namespace net {
class URLRequestContextGetter;
}

namespace suggestions {

class ImageFetcherDelegate;

// A class used to fetch server images asynchronously.
class ImageFetcherImpl : public suggestions::ImageFetcher {
 public:
  ImageFetcherImpl(net::URLRequestContextGetter* url_request_context,
                   base::SequencedWorkerPool* blocking_pool);
  ~ImageFetcherImpl() override;

  void SetImageFetcherDelegate(ImageFetcherDelegate* delegate) override;

  void StartOrQueueNetworkRequest(
      const GURL& url,
      const GURL& image_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback) override;

 private:
  scoped_ptr<image_fetcher::ImageFetcher> imageFetcher_;

  ImageFetcherDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace suggestions

#endif  // IOS_CHROME_BROWSER_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
