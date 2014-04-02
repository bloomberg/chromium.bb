// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace chrome {

BitmapFetcher::BitmapFetcher(const GURL& url,
                             BitmapFetcherDelegate* delegate)
    : url_(url),
      delegate_(delegate) {
}

BitmapFetcher::~BitmapFetcher() {}

void BitmapFetcher::Start(Profile* profile) {
  if (url_fetcher_ != NULL)
    return;

  url_fetcher_.reset(net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(profile->GetRequestContext());
  url_fetcher_->Start();
}

// Methods inherited from URLFetcherDelegate.

void BitmapFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    ReportFailure();
    return;
  }

  std::string image_data;
  source->GetResponseAsString(&image_data);
  image_decoder_ =
      new ImageDecoder(this, image_data, ImageDecoder::DEFAULT_CODEC);

  // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
  // with the data when it is done.
  scoped_refptr<base::MessageLoopProxy> task_runner =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI);
  image_decoder_->Start(task_runner);
}

void BitmapFetcher::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                               int64 current,
                                               int64 total) {
  // Do nothing here.
}

// Methods inherited from ImageDecoder::Delegate.

void BitmapFetcher::OnImageDecoded(const ImageDecoder* decoder,
                                   const SkBitmap& decoded_image) {
  // Make a copy of the bitmap which we pass back to the UI thread.
  scoped_ptr<SkBitmap> bitmap(new SkBitmap());
  decoded_image.deepCopyTo(bitmap.get());

  // Report success.
  delegate_->OnFetchComplete(url_, bitmap.get());
}

void BitmapFetcher::OnDecodeImageFailed(const ImageDecoder* decoder) {
  ReportFailure();
}

void BitmapFetcher::ReportFailure() {
  delegate_->OnFetchComplete(url_, NULL);
}

}  // namespace chrome
