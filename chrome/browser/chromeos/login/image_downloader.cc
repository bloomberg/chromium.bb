// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/image_downloader.h"

#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "content/browser/browser_thread.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/net/url_fetcher.h"

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
  image_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  if (!auth_token.empty()) {
    image_fetcher_->set_extra_request_headers(
        base::StringPrintf(kAuthorizationHeader, auth_token.c_str()));
  }
  image_fetcher_->Start();
}

void ImageDownloader::OnURLFetchComplete(const URLFetcher* source,
                                         const GURL& url,
                                         const net::URLRequestStatus& status,
                                         int response_code,
                                         const ResponseCookies& cookies,
                                         const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (response_code != 200) {
    LOG(ERROR) << "Response code is " << response_code;
    LOG(ERROR) << "Url is " << url.spec();
    LOG(ERROR) << "Data is " << data;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    return;
  }

  VLOG(1) << "Decoding the image...";
  std::vector<unsigned char> image_data(data.begin(), data.end());
  scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(delegate_,
                                                               image_data);
  image_decoder->Start();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace chromeos
