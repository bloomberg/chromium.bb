// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/image_downloader.h"

#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/browser/browser_thread.h"
#include "content/common/net/url_fetcher.h"

namespace chromeos {

namespace {

// Template for optional authorization header.
const char kAuthorizationHeader[] = "Authorization: GoogleLogin auth=%s";

}  // namespace

ImageDownloader::ImageDownloader(ImageDecoder::Delegate* delegate,
                                 const GURL& image_url,
                                 const std::string& auth_token)
    : delegate_(delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  image_fetcher_.reset(new URLFetcher(GURL(image_url), URLFetcher::GET, this));
  image_fetcher_->SetRequestContext(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  if (!auth_token.empty()) {
    image_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(kAuthorizationHeader, auth_token.c_str()));
  }
  image_fetcher_->Start();
}

ImageDownloader::~ImageDownloader() {}

void ImageDownloader::OnURLFetchComplete(const content::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string data;
  source->GetResponseAsString(&data);
  if (source->GetResponseCode() != 200) {
    LOG(ERROR) << "Response code is " << source->GetResponseCode();
    LOG(ERROR) << "Url is " << source->GetUrl().spec();
    LOG(ERROR) << "Data is " << data;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    return;
  }

  VLOG(1) << "Decoding the image...";
  scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(delegate_, data);
  image_decoder->Start();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace chromeos
