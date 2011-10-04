// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_CONSUMER_H_
#define CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_CONSUMER_H_
#pragma once

#include <string>

class GoogleServiceAuthError;

// An interface that defines the callbacks for objects to which
// GaiaOAuthFetcher can return data.
class GaiaOAuthConsumer {
 public:
  virtual ~GaiaOAuthConsumer() {}

  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) {}
  virtual void OnGetOAuthTokenFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuthGetAccessTokenSuccess(const std::string& token,
                                            const std::string& secret) {}
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) {}

  virtual void OnOAuthWrapBridgeSuccess(const std::string& service_scope,
                                        const std::string& token,
                                        const std::string& expires_in) {}
  virtual void OnOAuthWrapBridgeFailure(const std::string& service_scope,
                                        const GoogleServiceAuthError& error) {}

  virtual void OnUserInfoSuccess(const std::string& email) {}
  virtual void OnUserInfoFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuthLoginSuccess(const std::string& sid,
                                   const std::string& lsid,
                                   const std::string& auth) {}
  virtual void OnOAuthLoginFailure(const GoogleServiceAuthError& error) {}

  virtual void OnOAuthRevokeTokenSuccess() {}
  virtual void OnOAuthRevokeTokenFailure(const GoogleServiceAuthError& error) {}
};

#endif  // CHROME_BROWSER_NET_GAIA_GAIA_OAUTH_CONSUMER_H_
