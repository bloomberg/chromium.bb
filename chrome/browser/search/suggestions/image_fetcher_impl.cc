// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/image_fetcher_impl.h"

#include <string>

#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/image/image.h"

namespace suggestions {

ImageFetcherImpl::ImageFetcherImpl(
    net::URLRequestContextGetter* url_request_context)
    : delegate_(NULL), url_request_context_(url_request_context) {}

ImageFetcherImpl::~ImageFetcherImpl() {}

ImageFetcherImpl::ImageRequest::ImageRequest() : fetcher(NULL) {}

ImageFetcherImpl::ImageRequest::ImageRequest(chrome::BitmapFetcher* f)
    : fetcher(f) {}

ImageFetcherImpl::ImageRequest::ImageRequest(const ImageRequest& other) =
    default;

ImageFetcherImpl::ImageRequest::~ImageRequest() { delete fetcher; }

void ImageFetcherImpl::SetImageFetcherDelegate(
    image_fetcher::ImageFetcherDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void ImageFetcherImpl::StartOrQueueNetworkRequest(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const gfx::Image&)> callback) {
  // Before starting to fetch the image. Look for a request in progress for
  // |image_url|, and queue if appropriate.
  ImageRequestMap::iterator it = pending_net_requests_.find(image_url);
  if (it == pending_net_requests_.end()) {
    // |image_url| is not being fetched, so create a request and initiate
    // the fetch.
    ImageRequest request(new chrome::BitmapFetcher(image_url, this));
    request.url = url;
    request.callbacks.push_back(callback);
    request.fetcher->Init(
        url_request_context_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
    request.fetcher->Start();
    pending_net_requests_[image_url].swap(&request);
  } else {
    // Request in progress. Register as an interested callback.
    it->second.callbacks.push_back(callback);
  }
}

void ImageFetcherImpl::OnFetchComplete(const GURL& image_url,
                                       const SkBitmap* bitmap) {
  ImageRequestMap::iterator image_iter = pending_net_requests_.find(image_url);
  DCHECK(image_iter != pending_net_requests_.end());

  ImageRequest* request = &image_iter->second;

  // Here |bitmap| could be NULL. In this case an empty image is passed to the
  // callbacks and delegate. The pointer to the bitmap which is owned by the
  // BitmapFetcher ceases to exist after this function. The created gfx::Image
  // shares the pixels with the |bitmap|. The image is passed to the callbacks
  // and delegate that are run synchronously.
  gfx::Image image;
  if (bitmap != nullptr)
    image = gfx::Image::CreateFrom1xBitmap(*bitmap);

  for (const auto& callback : request->callbacks) {
    callback.Run(request->url, image);
  }

  // Inform the ImageFetcherDelegate.
  if (delegate_) {
    delegate_->OnImageFetched(request->url, image);
  }

  // Erase the completed ImageRequest.
  pending_net_requests_.erase(image_iter);
}

}  // namespace suggestions
