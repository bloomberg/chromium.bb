// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"

#include <UIKit/UIKit.h>

#include "base/threading/sequenced_worker_pool.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "ios/chrome/browser/net/image_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/image/image.h"

namespace suggestions {

ImageFetcherImpl::ImageFetcherImpl(
    net::URLRequestContextGetter* url_request_context,
    base::SequencedWorkerPool* blocking_pool) {
  imageFetcher_.reset(new ::ImageFetcher(blocking_pool));
  imageFetcher_->SetRequestContextGetter(url_request_context);
}

ImageFetcherImpl::~ImageFetcherImpl() {
}

void ImageFetcherImpl::SetImageFetcherDelegate(
    image_fetcher::ImageFetcherDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void ImageFetcherImpl::StartOrQueueNetworkRequest(
    const std::string& id,
    const GURL& image_url,
    base::Callback<void(const std::string&, const gfx::Image&)> callback) {
  if (image_url.is_empty()) {
    gfx::Image empty_image;
    callback.Run(id, empty_image);
    if (delegate_) {
      delegate_->OnImageFetched(id, empty_image);
    }
    return;
  }
  // Copy string reference so it's retained.
  const std::string fetch_id(id);
  ImageFetchedCallback fetcher_callback =
      ^(const GURL& original_url, int response_code, NSData* data) {
      if (data) {
        // Most likely always returns 1x images.
        UIImage* ui_image = [UIImage imageWithData:data scale:1];
        if (ui_image) {
          gfx::Image gfx_image(ui_image);
          callback.Run(fetch_id, gfx_image);
          if (delegate_) {
            delegate_->OnImageFetched(fetch_id, gfx_image);
          }
          return;
        }
      }
      gfx::Image empty_image;
      callback.Run(fetch_id, empty_image);
      if (delegate_) {
        delegate_->OnImageFetched(fetch_id, empty_image);
      }
  };
  imageFetcher_->StartDownload(image_url, fetcher_callback);
}

}  // namespace suggestions
