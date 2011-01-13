// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/cookie_fetcher.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace chromeos {

CookieFetcher::CookieFetcher(Profile* profile) : profile_(profile) {
  CHECK(profile_);
  client_login_handler_.reset(
      new ClientLoginResponseHandler(profile_->GetRequestContext()));
  issue_handler_.reset(
      new IssueResponseHandler(profile_->GetRequestContext()));
}

void CookieFetcher::AttemptFetch(const std::string& credentials) {
  VLOG(1) << "Getting auth token...";
  fetcher_.reset(client_login_handler_->Handle(credentials, this));
}

void CookieFetcher::OnURLFetchComplete(const URLFetcher* source,
                                       const GURL& url,
                                       const net::URLRequestStatus& status,
                                       int response_code,
                                       const ResponseCookies& cookies,
                                       const std::string& data) {
  if (status.is_success() && response_code == kHttpSuccess) {
    if (issue_handler_->CanHandle(url)) {
      VLOG(1) << "Handling auth token";
      fetcher_.reset(issue_handler_->Handle(data, this));
      return;
    }
  }
  BootTimesLoader::Get()->AddLoginTimeMarker("CookiesFetched", false);
  delete this;
}

}  // namespace chromeos
