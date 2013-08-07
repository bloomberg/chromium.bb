// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REFRESH_TOKEN_FETCHER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REFRESH_TOKEN_FETCHER_H_

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

// This class fetches an OAuth2 refresh token that is tied to a managed user ID
// and downscoped to a special scope for Chrome Sync for managed users.
// Fetching the token consists of the following steps:
//   1. Get an access token for the custodian from OAuth2TokenService
//      (either cached or fetched).
//   2. Call the IssueToken API to mint a scoped authorization code for a
//      refresh token for the managed user from the custodian's access token.
//   3. Exchange the authorization code for a refresh token for the managed
//      user and return it to the caller. The refresh token can only be used to
//      mint tokens with the special managed user Sync scope.
class ManagedUserRefreshTokenFetcher {
 public:
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* refresh_token */)>
      TokenCallback;

  static scoped_ptr<ManagedUserRefreshTokenFetcher> Create(
      OAuth2TokenService* oauth2_token_service,
      net::URLRequestContextGetter* context);

  virtual ~ManagedUserRefreshTokenFetcher();

  virtual void Start(const std::string& managed_user_id,
                     const std::string& device_name,
                     const TokenCallback& callback) = 0;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REFRESH_TOKEN_FETCHER_H_
