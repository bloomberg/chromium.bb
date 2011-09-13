// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/client_login_response_handler.h"

#include <algorithm>
#include <string>

#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "content/common/net/url_fetcher.h"
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
URLFetcher* ClientLoginResponseHandler::Handle(
    const std::string& to_process,
    URLFetcher::Delegate* catcher) {
  VLOG(1) << "Handling ClientLogin response!";
  payload_.assign(to_process);
  std::replace(payload_.begin(), payload_.end(), '\n', '&');
  payload_.append(kService);

  URLFetcher* fetcher =
      new URLFetcher(GURL(GaiaUrls::GetInstance()->issue_auth_token_url()),
                     URLFetcher::POST,
                     catcher);
  fetcher->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher->set_upload_data("application/x-www-form-urlencoded", payload_);
  if (getter_) {
    VLOG(1) << "Fetching " << GaiaUrls::GetInstance()->issue_auth_token_url();
    fetcher->set_request_context(getter_);
    fetcher->Start();
  }
  return fetcher;
}

}  // namespace chromeos
