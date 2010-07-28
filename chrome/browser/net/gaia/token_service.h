// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The TokenService will supply authentication tokens for any service that
// needs it, such as sync.

#ifndef CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
#define CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include "base/scoped_ptr.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/test/testing_profile.h"

class URLRequestContextGetter;

// The TokenService is a Profile member, so all calls are expected
// from the UI thread.
class TokenService : public GaiaAuthConsumer {
 public:
   TokenService() {}

  // Notification classes
  class TokenAvailableDetails {
   public:
    TokenAvailableDetails() {}
    TokenAvailableDetails(const std::string& service,
                          const std::string& token)
        : service_(service), token_(token) {}
    const std::string& service() const { return service_; }
    const std::string& token() const { return token_; }
   private:
    std::string service_;
    std::string token_;
  };

  class TokenRequestFailedDetails {
   public:
    TokenRequestFailedDetails() {}
    TokenRequestFailedDetails(const std::string& service,
                              const GaiaAuthError& error)
        : service_(service), error_(error) {}
    const std::string& service() const { return service_; }
    const GaiaAuthError& error() const { return error_; }
   private:
    std::string service_;
    GaiaAuthError error_;
  };

  // Initialize this token service with a request source
  // (usually from a GaiaAuthConsumer constant), a getter and
  // results from ClientLogin.
  void Initialize(
      const char* const source,
      URLRequestContextGetter* getter,
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // For legacy services with their own auth routines, they can just read
  // the LSID out directly. Deprecated.
  const bool HasLsid() const;
  const std::string& GetLsid() const;

  // On login, StartFetchingTokens() should be called to kick off token
  // fetching.
  // Tokens will be fetched for all services(sync, talk) in the background.
  // Results come back via event channel. Services can also poll.
  void StartFetchingTokens();
  const bool HasTokenForService(const char* const service) const;
  const std::string& GetTokenForService(const char* const service) const;

  // Callbacks from the fetchers.
  void OnIssueAuthTokenSuccess(const std::string& service,
                               const std::string& auth_token);
  void OnIssueAuthTokenFailure(const std::string& service,
                               const GaiaAuthError& error);

 private:
  // Did we get a proper LSID?
  const bool AreCredentialsValid() const;

  // Gaia request source for Gaia accounting.
  std::string source_;
  // Credentials from ClientLogin for Issuing auth tokens.
  GaiaAuthConsumer::ClientLoginResult credentials_;
  scoped_ptr<GaiaAuthenticator2> sync_token_fetcher_;
  scoped_ptr<GaiaAuthenticator2> talk_token_fetcher_;
  // Map from service to token.
  std::map<std::string, std::string> token_map_;

  DISALLOW_COPY_AND_ASSIGN(TokenService);
};

#endif  // CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
