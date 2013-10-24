// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_GOOGLE_AUTO_LOGIN_HELPER_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_GOOGLE_AUTO_LOGIN_HELPER_H_

#include "chrome/browser/signin/ubertoken_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class GaiaAuthFetcher;
class Profile;

// Logs in the current signed in user into Google services. Populates the cookie
// jar with Google credentials of the signed in user.
class GoogleAutoLoginHelper : public GaiaAuthConsumer,
                              public UbertokenConsumer {
 public:
  explicit GoogleAutoLoginHelper(Profile* profile);
  virtual ~GoogleAutoLoginHelper();

  void LogIn();
  void LogIn(const std::string& account_id);

  // Overridden from GaiaAuthConsumer.
  virtual void OnMergeSessionSuccess(const std::string& data) OVERRIDE;
  virtual void OnMergeSessionFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  // Overridden from UbertokenConsumer.
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

 private:
  Profile* profile_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  scoped_ptr<UbertokenFetcher> uber_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GoogleAutoLoginHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_GOOGLE_AUTO_LOGIN_HELPER_H_
