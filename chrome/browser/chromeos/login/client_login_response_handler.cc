// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/client_login_response_handler.h"

#include <algorithm>
#include <string>

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"

namespace chromeos {

// By setting "service=gaia", we get an uber-auth-token back.
const char ClientLoginResponseHandler::kService[] = "service=gaia";

// Overridden from AuthResponseHandler.
bool ClientLoginResponseHandler::CanHandle(const GURL& url) {
  return (url.spec().find(GaiaUrls::GetInstance()->client_login_url()) !=
          std::string::npos);
}

// Overridden from AuthResponseHandler.
content::URLFetcher* ClientLoginResponseHandler::Handle(
    const std::string& to_process,
    content::URLFetcherDelegate* catcher) {
  VLOG(1) << "Handling ClientLogin response!";
  payload_.assign(to_process);
  std::replace(payload_.begin(), payload_.end(), '\n', '&');
  payload_.append(kService);

  content::URLFetcher* fetcher = content::URLFetcher::Create(
      GURL(GaiaUrls::GetInstance()->issue_auth_token_url()),
      content::URLFetcher::POST,
      catcher);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->SetUploadData("application/x-www-form-urlencoded", payload_);
  if (getter_) {
    VLOG(1) << "Fetching " << GaiaUrls::GetInstance()->issue_auth_token_url();
    fetcher->SetRequestContext(getter_);
    fetcher->Start();
  }
  return fetcher;
}

}  // namespace chromeos
