// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/notification_bitmap_fetcher.h"

#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"

namespace notifier {

NotificationBitmapFetcher::NotificationBitmapFetcher(
    const GURL& url,
    NotificationBitmapFetcherDelegate* delegate)
    : url_(url), delegate_(delegate) {}

NotificationBitmapFetcher::~NotificationBitmapFetcher() {}

void NotificationBitmapFetcher::Start() {
  if (url_fetcher_ == NULL) {
    url_fetcher_.reset(
        net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
  }
  url_fetcher_->Start();
}

void NotificationBitmapFetcher::SetURLFetcherForTest(
    scoped_ptr<net::URLFetcher> url_fetcher) {
  url_fetcher_ = url_fetcher.Pass();
}

// Methods inherited from URLFetcherDelegate.

void NotificationBitmapFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string image_data;

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Copy the data into the string.  Keep in mind it may have embedded nulls.
  source->GetResponseAsString(&image_data);

  // Handle fetch failure.  If it failed, set failed to true, and fire
  // notification to listeners.
  if (image_data.length() == 0) {
    OnDecodeImageFailed(NULL);
    return;
  }

  // Create an ImageDecoder with the data and assign it to the refptr.
  image_decoder_ = new ImageDecoder(this, image_data,
                                    ImageDecoder::DEFAULT_CODEC);

  // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
  // with the data when it is done.
  scoped_refptr<base::MessageLoopProxy> task_runner =
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI);
  image_decoder_->Start(task_runner);
}

void NotificationBitmapFetcher::OnURLFetchDownloadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
  // Do nothing here.
}

// Methods inherited from ImageDecoder::Delegate.

void NotificationBitmapFetcher::OnImageDecoded(
    const ImageDecoder* decoder, const SkBitmap& decoded_image) {
  // Make a copy of the bitmap which we pass back to the UI thread.
  bitmap_.reset(new SkBitmap());
  decoded_image.deepCopyTo(bitmap_.get(), decoded_image.getConfig());

  // Report success.
  delegate_->OnFetchComplete(bitmap_.get());
}

void NotificationBitmapFetcher::OnDecodeImageFailed(
    const ImageDecoder* decoder) {

  // Report failure.
  delegate_->OnFetchComplete(NULL);
}

}  // namespace notifier
