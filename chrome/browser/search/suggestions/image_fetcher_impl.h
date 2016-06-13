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
#include "components/image_fetcher/image_data_fetcher.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace net {
class URLRequestContextGetter;
}

// TODO(markusheintz): Move the ImageFetcherImpl to components/image_fetcher
namespace suggestions {

// image_fetcher::ImageFetcher implementation.
class ImageFetcherImpl : public image_fetcher::ImageFetcher {
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
  using CallbackVector =
      std::vector<base::Callback<void(const std::string&, const gfx::Image&)>>;

  // State related to an image fetch (id, pending callbacks).
  struct ImageRequest {
    ImageRequest();
    ImageRequest(const ImageRequest& other);
    ~ImageRequest();

    void swap(ImageRequest* other) {
      std::swap(id, other->id);
      std::swap(callbacks, other->callbacks);
    }

    std::string id;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  using ImageRequestMap = std::map<const GURL, ImageRequest>;

  // Processes image URL fetched events. This is the continuation method used
  // for creating callbacks that are passed to the ImageDataFetcher.
  void OnImageURLFetched(const GURL& image_url, const std::string& image_data);

  // Processes image decoded events. This is the continuation method used for
  // creating callbacks that are passed to the ImageDecoder.
  void OnImageDecoded(const GURL& image_url, const gfx::Image& image);


  image_fetcher::ImageFetcherDelegate* delegate_;

  net::URLRequestContextGetter* url_request_context_;

  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  std::unique_ptr<image_fetcher::ImageDataFetcher> image_data_fetcher_;

  // Map from each image URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ImageRequestMap pending_net_requests_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_FETCHER_IMPL_H_
