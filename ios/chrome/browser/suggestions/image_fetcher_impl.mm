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
    const GURL& url,
    const GURL& image_url,
    base::Callback<void(const GURL&, const gfx::Image&)> callback) {
  if (image_url.is_empty()) {
    gfx::Image empty_image;
    callback.Run(url, empty_image);
    if (delegate_) {
      delegate_->OnImageFetched(url, empty_image);
    }
    return;
  }
  // Copy url reference so it's retained.
  const GURL page_url(url);
  ImageFetchedCallback fetcher_callback =
      ^(const GURL& original_url, int response_code, NSData* data) {
      if (data) {
        // Most likely always returns 1x images.
        UIImage* ui_image = [UIImage imageWithData:data scale:1];
        if (ui_image) {
          gfx::Image gfx_image(ui_image);
          callback.Run(page_url, gfx_image);
          if (delegate_) {
            delegate_->OnImageFetched(page_url, gfx_image);
          }
          return;
        }
      }
      gfx::Image empty_image;
      callback.Run(page_url, empty_image);
      if (delegate_) {
        delegate_->OnImageFetched(page_url, empty_image);
      }
  };
  imageFetcher_->StartDownload(image_url, fetcher_callback);
}

}  // namespace suggestions
