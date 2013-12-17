// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_ACCOUNT_ID_HELPER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_ACCOUNT_ID_HELPER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

class CookieSettings;
class GaiaAuthFetcher;
class Profile;

// The helper class for managing the obfuscated account ID of the primary
// account. It fetches the ID when user first signs into Chrome or when user
// opens a connected Chrome profile without an obfuscated account ID, and stores
// the ID in the profile preference.
class SigninAccountIdHelper
    : public content::NotificationObserver,
      public OAuth2TokenService::Observer {
 public:
  explicit SigninAccountIdHelper(Profile* profile);
  virtual ~SigninAccountIdHelper();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& email) OVERRIDE;

  // Disables network requests for testing to avoid messing up with irrelevant
  // tests.
  static void SetDisableForTest(bool disable_for_test);

 private:
  // Invoked when receiving the response for |account_id_fetcher_|.
  void OnPrimaryAccountIdFetched(const std::string& account_id);

  // Helper class for fetching the obfuscated account ID.
  class AccountIdFetcher;
  scoped_ptr<AccountIdFetcher> id_fetcher_;

  static bool disable_for_test_;

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SigninAccountIdHelper);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_ACCOUNT_ID_HELPER_H_
