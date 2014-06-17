// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_REFRESH_TOKEN_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_REFRESH_TOKEN_FETCHER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

class GoogleServiceAuthError;
class OAuth2TokenService;

namespace net {
class URLRequestContextGetter;
}

// This class fetches an OAuth2 refresh token that is tied to a supervised user
// ID and downscoped to a special scope for Chrome Sync for supervised users.
// Fetching the token consists of the following steps:
//   1. Get an access token for the custodian from OAuth2TokenService
//      (either cached or fetched).
//   2. Call the IssueToken API to mint a scoped authorization code for a
//      refresh token for the supervised user from the custodian's access token.
//   3. Exchange the authorization code for a refresh token for the supervised
//      user and return it to the caller. The refresh token can only be used to
//      mint tokens with the special supervised user Sync scope.
class SupervisedUserRefreshTokenFetcher {
 public:
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* refresh_token */)>
      TokenCallback;

  static scoped_ptr<SupervisedUserRefreshTokenFetcher> Create(
      OAuth2TokenService* oauth2_token_service,
      const std::string& account_id,
      net::URLRequestContextGetter* context);

  virtual ~SupervisedUserRefreshTokenFetcher();

  virtual void Start(const std::string& supervised_user_id,
                     const std::string& device_name,
                     const TokenCallback& callback) = 0;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_REFRESH_TOKEN_FETCHER_H_
