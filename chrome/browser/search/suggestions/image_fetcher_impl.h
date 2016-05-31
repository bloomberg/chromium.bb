// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "components/image_fetcher/image_fetcher.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace net {
class URLRequestContextGetter;
}

namespace suggestions {

// image_fetcher::ImageFetcher implementation.
class ImageFetcherImpl : public image_fetcher::ImageFetcher,
                         public chrome::BitmapFetcherDelegate {
 public:
  explicit ImageFetcherImpl(net::URLRequestContextGetter* url_request_context);
  ~ImageFetcherImpl() override;

  void SetImageFetcherDelegate(
      image_fetcher::ImageFetcherDelegate* delegate) override;

  void StartOrQueueNetworkRequest(
      const std::string& id,
      const GURL& image_url,
      base::Callback<void(const std::string&, const gfx::Image&)> callback)
      override;

 private:
  // Inherited from BitmapFetcherDelegate.
  void OnFetchComplete(const GURL& image_url, const SkBitmap* bitmap) override;

  using CallbackVector =
      std::vector<base::Callback<void(const std::string&, const gfx::Image&)>>;

  // State related to an image fetch (id, image_url, fetcher, pending
  // callbacks).
  struct ImageRequest {
    ImageRequest();
    // Struct takes ownership of |f|.
    explicit ImageRequest(chrome::BitmapFetcher* f);
    ImageRequest(const ImageRequest& other);
    ~ImageRequest();

    void swap(ImageRequest* other) {
      std::swap(id, other->id);
      std::swap(image_url, other->image_url);
      std::swap(callbacks, other->callbacks);
      std::swap(fetcher, other->fetcher);
    }

    std::string id;
    GURL image_url;
    chrome::BitmapFetcher* fetcher;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  using ImageRequestMap = std::map<const GURL, ImageRequest>;

  // Map from each image URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ImageRequestMap pending_net_requests_;

  image_fetcher::ImageFetcherDelegate* delegate_;

  net::URLRequestContextGetter* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
