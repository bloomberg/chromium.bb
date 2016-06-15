// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_data_fetcher.h"

#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace image_fetcher {

// An active image URL fetcher request. The class contains any related request
// state and logic for handling a single image url request.
class ImageDataFetcher::ImageDataFetcherRequest
    : public net::URLFetcherDelegate {
 public:
  ImageDataFetcherRequest(const GURL& url,
                          const ImageDataFetcherCallback& callback,
                          ImageDataFetcher* image_url_fetcher);
  ~ImageDataFetcherRequest() override {}

  // Sends the URL requests.
  void Start(net::URLRequestContextGetter* request_context);

 private:
  // Method inherited from URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // The URL of the image to fetch.
  const GURL url_;

  // The callback to run after the image data was fetched. The callback will
  // be run even if the image data could not be fetched successfully.
  ImageDataFetcherCallback callback_;

  // The ImageDataFetcher that owns the ImageDataFetcherRequest.
  ImageDataFetcher* image_url_fetcher_;

  std::unique_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataFetcherRequest);
};

ImageDataFetcher::ImageDataFetcherRequest::ImageDataFetcherRequest(
    const GURL& url,
    const ImageDataFetcherCallback& callback,
    ImageDataFetcher* image_url_fetcher)
    : url_(url),
      callback_(callback),
      image_url_fetcher_(image_url_fetcher) {
}

void ImageDataFetcher::ImageDataFetcherRequest::Start(
    net::URLRequestContextGetter* request_context) {
  DCHECK(!url_fetcher_);

  url_fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::GET, this);
  url_fetcher_->SetRequestContext(request_context);
  url_fetcher_->Start();
}

void ImageDataFetcher::ImageDataFetcherRequest::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // An empty string is passed to the callback in case on an unsuccessfull URL
  // request.
  std::string image_data;
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS) {
    source->GetResponseAsString(&image_data);
  }
  callback_.Run(image_data);

  // Remove the completed ImageDataFetcherRequest from the internal request
  // queue. This must be last in the method.
  image_url_fetcher_->RemoveImageDataFetcherRequest(url_);
}

ImageDataFetcher::ImageDataFetcher(
    net::URLRequestContextGetter* url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter) {}

ImageDataFetcher::~ImageDataFetcher() {}

void ImageDataFetcher::FetchImageData(
    const GURL& url, const ImageDataFetcherCallback& callback) {
  std::unique_ptr<ImageDataFetcherRequest> request(
      new ImageDataFetcherRequest(url, callback, this));
  request->Start(url_request_context_getter_.get());
  pending_requests_[url] = std::move(request);
}

void ImageDataFetcher::RemoveImageDataFetcherRequest(const GURL& image_url) {
  size_t count = pending_requests_.erase(image_url);
  DCHECK(count);
}

}  // namespace image_fetcher
