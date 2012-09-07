// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_CONSUMER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_CONSUMER_H_

#include <string>

class GoogleServiceAuthError;

// An interface that defines the callbacks for consumers to which
// OAuth2MintTokenFetcher can return results.
class OAuth2MintTokenConsumer {
 public:
  virtual ~OAuth2MintTokenConsumer() {}

  virtual void OnMintTokenSuccess(const std::string& access_token) {}
  virtual void OnMintTokenFailure(const GoogleServiceAuthError& error) {}
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_MINT_TOKEN_CONSUMER_H_
