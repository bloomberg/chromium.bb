// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_UBERTOKEN_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_UBERTOKEN_FETCHER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"

// Allow to retrieves an uber-auth token for the user. This class uses the
// |TokenService| and considers that the user is already logged in. It will then
// retrieve the OAuth2 refresh token, use it to generate an OAuth2 access token
// and finally use this access token to generate the uber-auth token.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time.

class GoogleServiceAuthError;
class Profile;

// Callback for the |UbertokenFetcher| class.
class UbertokenConsumer {
 public:
  UbertokenConsumer() {}
  virtual ~UbertokenConsumer() {}
  virtual void OnUbertokenSuccess(const std::string& token) {}
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) {}
};

// Allows to retrieve an uber-auth token.
class UbertokenFetcher : public content::NotificationObserver,
                         public gaia::GaiaOAuthClient::Delegate,
                         public GaiaAuthConsumer {
 public:
  UbertokenFetcher(Profile* profile, UbertokenConsumer* consumer);
  virtual ~UbertokenFetcher();

  // Start fetching the token.
  void StartFetchingToken();

  // Overriden from gaia::GaiaOAuthClient::Delegate:
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // Overriden from GaiaAuthConsumer
  virtual void OnUberAuthTokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUberAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // Overriden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void StartFetchingUbertoken();

  Profile* profile_;
  UbertokenConsumer* consumer_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UbertokenFetcher);
};

#endif  // CHROME_BROWSER_SIGNIN_UBERTOKEN_FETCHER_H_
