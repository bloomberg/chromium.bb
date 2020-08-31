// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

class GoogleServiceAuthError;

// An interface that defines the callbacks for consumers to which
// OAuth2AccessTokenFetcher can return results.
class OAuth2AccessTokenConsumer {
 public:
  // Structure representing information contained in OAuth2 access token.
  struct TokenResponse {
    TokenResponse() = default;
    TokenResponse(const std::string& access_token,
                  const base::Time& expiration_time,
                  const std::string& id_token);

    // OAuth2 access token.
    std::string access_token;

    // The date until which the |access_token| can be used.
    // This value has a built-in safety margin, so it can be used as-is.
    base::Time expiration_time;

    // Contains extra information regarding the user's currently registered
    // services.
    std::string id_token;
  };

  OAuth2AccessTokenConsumer() = default;
  virtual ~OAuth2AccessTokenConsumer();

  // Success callback.
  virtual void OnGetTokenSuccess(const TokenResponse& token_response);

  // Failure callback.
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenConsumer);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
