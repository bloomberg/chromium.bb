// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ISSUE_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ISSUE_RESPONSE_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// Handles responses to a fetch executed upon the Google Accounts IssueAuthToken
// endpoint.  The token that's sent back in the response body is used as an
// URL query parameter in a request that, ultimately, results in a full set
// of authorization cookies for Google services being left in the cookie jar
// associated with |getter_|.
class IssueResponseHandler : public AuthResponseHandler {
 public:
  explicit IssueResponseHandler(net::URLRequestContextGetter* getter)
      : getter_(getter) {}
  virtual ~IssueResponseHandler() {}

  // Overridden from AuthResponseHandler.
  virtual bool CanHandle(const GURL& url) OVERRIDE;

  // Overridden from AuthResponseHandler.
  // Takes in a response from IssueAuthToken, formats into an appropriate query
  // to sent to TokenAuth, and issues said query.  |catcher| will receive
  // the response to the fetch.  This fetch will follow redirects, which is
  // necesary to support GAFYD and corp accounts.
  virtual content::URLFetcher* Handle(
      const std::string& to_process,
      content::URLFetcherDelegate* catcher) OVERRIDE;

  // exposed for testing
  std::string token_url() { return token_url_; }

  // Builds a TokenAuth URL using the specified authorization token.
  static std::string BuildTokenAuthUrlWithToken(const std::string& token);

 private:
  std::string token_url_;
  net::URLRequestContextGetter* getter_;
  DISALLOW_COPY_AND_ASSIGN(IssueResponseHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ISSUE_RESPONSE_HANDLER_H_
