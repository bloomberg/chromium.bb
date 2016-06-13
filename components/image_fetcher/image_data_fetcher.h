// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace image_fetcher {

class ImageDataFetcher {
 public:
  using ImageDataFetcherCallback =
      base::Callback<void(const std::string& image_data)>;

  explicit ImageDataFetcher(
      net::URLRequestContextGetter* url_request_context_getter);
  ~ImageDataFetcher();

  // Fetches the raw image bytes from the given |image_url| and calls the given
  // |callback|. The callback is run even if fetching the URL fails. In case
  // of an error an empty string is passed to the callback.
  void FetchImageData(const GURL& image_url,
                      const ImageDataFetcherCallback& callback);

 private:
  class ImageDataFetcherRequest;

  // Removes the ImageDataFetcherRequest for the given |image_url| from the
  // internal request queue.
  void RemoveImageDataFetcherRequest(const GURL& image_url);

  // All active image url requests.
  std::map<const GURL, std::unique_ptr<ImageDataFetcherRequest>>
      pending_requests_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_
