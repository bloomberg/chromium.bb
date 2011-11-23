// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
#define CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
#pragma once

#include <string>

class GoogleServiceAuthError;

// An interface that defines the callbacks for consumers to which
// OAuth2AccessTokenFetcher can return results.
class OAuth2AccessTokenConsumer {
 public:
  virtual ~OAuth2AccessTokenConsumer() {}

  virtual void OnGetTokenSuccess(const std::string& access_token) {}
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) {}
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_CONSUMER_H_
