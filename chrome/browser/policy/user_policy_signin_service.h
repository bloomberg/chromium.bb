// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

class OAuth2AccessTokenFetcher;
class Profile;

namespace base {
class Time;
}

namespace policy {

class UserCloudPolicyManager;

// The UserPolicySigninService tracks when user signin/signout actions occur and
// initializes/shuts down the UserCloudPolicyManager as required. This class is
// not used on ChromeOS because UserCloudPolicyManager initialization is handled
// via LoginUtils, since it must happen before profile creation.
class UserPolicySigninService
    : public ProfileKeyedService,
      public OAuth2AccessTokenConsumer,
      public content::NotificationObserver {
 public:
  // Creates a UserPolicySigninService associated with the passed |profile|.
  UserPolicySigninService(Profile* profile, UserCloudPolicyManager* manager);
  virtual ~UserPolicySigninService();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2AccessTokenConsumer implementation.
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

 private:
  // Initializes the UserCloudPolicyManager to reflect the currently-signed-in
  // user.
  void ConfigureUserCloudPolicyManager();

  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server.
  void RegisterCloudPolicyService();

  // Weak pointer to the profile this service is associated with.
  Profile* profile_;

  content::NotificationRegistrar registrar_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  // Weak pointer to the UserCloudPolicyManager (allows dependency injection
  // for tests).
  UserCloudPolicyManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicySigninService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_SIGNIN_SERVICE_H_
