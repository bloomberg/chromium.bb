// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"

#include <UIKit/UIKit.h>

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#import "ios/web/public/image_fetcher/image_data_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/image/image.h"

namespace suggestions {

ImageFetcherImpl::ImageFetcherImpl(
    net::URLRequestContextGetter* url_request_context,
    base::SequencedWorkerPool* blocking_pool)
    : image_fetcher_(base::MakeUnique<web::ImageDataFetcher>(blocking_pool)) {
  image_fetcher_->SetRequestContextGetter(url_request_context);
}

ImageFetcherImpl::~ImageFetcherImpl() {
}

void ImageFetcherImpl::SetImageFetcherDelegate(
    image_fetcher::ImageFetcherDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void ImageFetcherImpl::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  // Not implemented - will be obsolete once iOS also uses
  // image_fetcher::ImageDataFetcher.
  NOTREACHED();
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
  // If image_fetcher_ is destroyed the request will be cancelled and this block
  // will never be called. A reference to delegate_ can be kept.
  web::ImageFetchedCallback fetcher_callback =
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
  image_fetcher_->StartDownload(image_url, fetcher_callback);
}

}  // namespace suggestions
