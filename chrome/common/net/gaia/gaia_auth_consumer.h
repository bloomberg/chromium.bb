// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTH_CONSUMER_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTH_CONSUMER_H_
#pragma once

#include <string>
#include <map>
#include <vector>

class GoogleServiceAuthError;

namespace net {
typedef std::vector<std::string> ResponseCookies;
}

typedef std::map<std::string, std::string> UserInfoMap;

// An interface that defines the callbacks for objects that
// GaiaAuthFetcher can return data to.
class GaiaAuthConsumer {
 public:
  struct ClientLoginResult {
    ClientLoginResult();
    ClientLoginResult(const std::string& new_sid,
                      const std::string& new_lsid,
                      const std::string& new_token,
                      const std::string& new_data);
    ~ClientLoginResult();

    bool operator==(const ClientLoginResult &b) const;

    std::string sid;
    std::string lsid;
    std::string token;
    // TODO(chron): Remove the data field later. Don't use it if possible.
    std::string data;  // Full contents of ClientLogin return.
    bool two_factor;  // set to true if there was a TWO_FACTOR "failure".
  };

  virtual ~GaiaAuthConsumer() {}

  virtual void OnClientLoginSuccess(const ClientLoginResult& result) {}
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error) {}

  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) {}
  virtual void OnIssueAuthTokenFailure(const std::string& service,
                                       const GoogleServiceAuthError& error) {}

  virtual void OnOAuthLoginTokenSuccess(const std::string& refresh_token,
                                        const std::string& access_token,
                                        int expires_in_secs) {}
  virtual void OnOAuthLoginTokenFailure(const GoogleServiceAuthError& error) {}

  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) {}
  virtual void OnGetUserInfoFailure(const GoogleServiceAuthError& error) {}

  virtual void OnTokenAuthSuccess(const net::ResponseCookies& cookies,
                                  const std::string& data) {}
  virtual void OnTokenAuthFailure(const GoogleServiceAuthError& error) {}

  virtual void OnUberAuthTokenSuccess(const std::string& token) {}
  virtual void OnUberAuthTokenFailure(const GoogleServiceAuthError& error) {}

  virtual void OnMergeSessionSuccess(const std::string& data) {}
  virtual void OnMergeSessionFailure(const GoogleServiceAuthError& error) {}
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTH_CONSUMER_H_
