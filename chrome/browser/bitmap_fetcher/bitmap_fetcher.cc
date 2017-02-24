// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace chrome {

BitmapFetcher::BitmapFetcher(
    const GURL& url,
    BitmapFetcherDelegate* delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : url_(url), delegate_(delegate), traffic_annotation_(traffic_annotation) {}

BitmapFetcher::~BitmapFetcher() {
}

void BitmapFetcher::Init(net::URLRequestContextGetter* request_context,
                         const std::string& referrer,
                         net::URLRequest::ReferrerPolicy referrer_policy,
                         int load_flags) {
  if (url_fetcher_ != NULL)
    return;

  url_fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::GET, this,
                                         traffic_annotation_);
  url_fetcher_->SetRequestContext(request_context);
  url_fetcher_->SetReferrer(referrer);
  url_fetcher_->SetReferrerPolicy(referrer_policy);
  url_fetcher_->SetLoadFlags(load_flags);
}

void BitmapFetcher::Start() {
  if (url_fetcher_)
    url_fetcher_->Start();
}

// Methods inherited from URLFetcherDelegate.

void BitmapFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    ReportFailure();
    return;
  }

  std::string image_data;
  source->GetResponseAsString(&image_data);

  // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
  // with the data when it is done.
  ImageDecoder::Start(this, image_data);
}

// Methods inherited from ImageDecoder::ImageRequest.

void BitmapFetcher::OnImageDecoded(const SkBitmap& decoded_image) {
  // Report success.
  delegate_->OnFetchComplete(url_, &decoded_image);
}

void BitmapFetcher::OnDecodeImageFailed() {
  ReportFailure();
}

void BitmapFetcher::ReportFailure() {
  delegate_->OnFetchComplete(url_, NULL);
}

}  // namespace chrome
