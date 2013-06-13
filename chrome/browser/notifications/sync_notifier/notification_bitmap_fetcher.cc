// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/notification_bitmap_fetcher.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace notifier {

NotificationBitmapFetcher::NotificationBitmapFetcher(
    const GURL& url,
    NotificationBitmapFetcherDelegate* delegate)
    : url_(url), delegate_(delegate) {}

NotificationBitmapFetcher::~NotificationBitmapFetcher() {}

void NotificationBitmapFetcher::Start(Profile* profile) {
  url_fetcher_.reset(
      net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
  // The RequestContext is coming from the current profile.
  // TODO(petewil): Make sure this is the right profile to use.
  // It seems to work, but we might prefer to use a blank profile with
  // no cookies.
  url_fetcher_->SetRequestContext(profile->GetRequestContext());
  url_fetcher_->Start();
}

// Methods inherited from URLFetcherDelegate.

void NotificationBitmapFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    OnDecodeImageFailed(NULL);
    return;
  }

  std::string image_data;
  source->GetResponseAsString(&image_data);
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
  delegate_->OnFetchComplete(url_, bitmap_.get());
}

void NotificationBitmapFetcher::OnDecodeImageFailed(
    const ImageDecoder* decoder) {

  // Report failure.
  delegate_->OnFetchComplete(url_, NULL);
}

}  // namespace notifier
