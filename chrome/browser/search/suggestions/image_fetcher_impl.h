// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_

#include <map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "components/image_fetcher/image_fetcher.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace suggestions {

// A class used to fetch server images. It can be called from any thread and the
// callback will be called on the thread which initiated the fetch.
class ImageFetcherImpl : public image_fetcher::ImageFetcher,
                         public chrome::BitmapFetcherDelegate {
 public:
  explicit ImageFetcherImpl(net::URLRequestContextGetter* url_request_context);
  ~ImageFetcherImpl() override;

  void SetImageFetcherDelegate(
      image_fetcher::ImageFetcherDelegate* delegate) override;

  void StartOrQueueNetworkRequest(
      const GURL& url,
      const GURL& image_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback) override;

 private:
  // Inherited from BitmapFetcherDelegate.
  void OnFetchComplete(const GURL& image_url, const SkBitmap* bitmap) override;

  typedef std::vector<base::Callback<void(const GURL&, const SkBitmap*)> >
      CallbackVector;

  // State related to an image fetch (associated website url, image_url,
  // fetcher, pending callbacks).
  struct ImageRequest {
    ImageRequest();
    // Struct takes ownership of |f|.
    explicit ImageRequest(chrome::BitmapFetcher* f);
    ImageRequest(const ImageRequest& other);
    ~ImageRequest();

    void swap(ImageRequest* other) {
      std::swap(url, other->url);
      std::swap(image_url, other->image_url);
      std::swap(callbacks, other->callbacks);
      std::swap(fetcher, other->fetcher);
    }

    GURL url;
    GURL image_url;
    chrome::BitmapFetcher* fetcher;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  typedef std::map<const GURL, ImageRequest> ImageRequestMap;

  // Map from each image URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ImageRequestMap pending_net_requests_;

  image_fetcher::ImageFetcherDelegate* delegate_;

  net::URLRequestContextGetter* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
