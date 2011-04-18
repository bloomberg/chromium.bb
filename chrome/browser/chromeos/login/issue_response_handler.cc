// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/issue_response_handler.h"

#include <string>

#include "base/stringprintf.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/net/url_fetcher.h"
#include "net/base/load_flags.h"

namespace chromeos {

// Overridden from AuthResponseHandler.
bool IssueResponseHandler::CanHandle(const GURL& url) {
  return (url.spec().find(AuthResponseHandler::kIssueAuthTokenUrl) !=
          std::string::npos);
}

// Overridden from AuthResponseHandler.
URLFetcher* IssueResponseHandler::Handle(
    const std::string& to_process,
    URLFetcher::Delegate* catcher) {
  VLOG(1) << "Handling IssueAuthToken response";
  token_url_.assign(base::StringPrintf("%s%s",
      AuthResponseHandler::kTokenAuthUrl, to_process.c_str()));
  URLFetcher* fetcher =
      new URLFetcher(GURL(token_url_), URLFetcher::GET, catcher);
  fetcher->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES);
  if (getter_) {
    VLOG(1) << "Fetching " << AuthResponseHandler::kTokenAuthUrl;
    fetcher->set_request_context(getter_);
    fetcher->Start();
  }
  return fetcher;
}

}  // namespace chromeos
