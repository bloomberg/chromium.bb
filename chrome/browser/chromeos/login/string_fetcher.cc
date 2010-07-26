// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/string_fetcher.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/profile_manager.h"
#include "googleurl/src/gurl.h"

StringFetcher::StringFetcher(const std::string& url_str) {
  GURL url(url_str);

  DCHECK(url.is_valid());
  if (!url.is_valid()) {
    response_code_ = 404;
    return;
  }

  if (url.SchemeIsFile()) {
    LOG(INFO) << url.path();
    if (file_util::ReadFileToString(FilePath(url.path()), &result_)) {
      response_code_ = 200;
    } else {
      response_code_ = 404;
    }
    return;
  }
  url_fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
  url_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  url_fetcher_->Start();
}

void StringFetcher::OnURLFetchComplete(const URLFetcher* source,
                                       const GURL& url,
                                       const URLRequestStatus& status,
                                       int response_code,
                                       const ResponseCookies& cookies,
                                       const std::string& data) {
  response_code_ = response_code;
  if (response_code != 200) {
    LOG(ERROR) << "Response code is " << response_code;
    LOG(ERROR) << "Url is " << url.spec();
    LOG(ERROR) << "Data is " << data;
    return;
  }
  result_ = data;
}
