// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_signin_manager.h"

#include "base/callback_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/account_tracker_service.h"

FakeSigninManagerBase::FakeSigninManagerBase(Profile* profile)
    : SigninManagerBase(
          ChromeSigninClientFactory::GetForProfile(profile),
          AccountTrackerServiceFactory::GetForProfile(profile)) {}

FakeSigninManagerBase::~FakeSigninManagerBase() {
}

// static
scoped_ptr<KeyedService> FakeSigninManagerBase::Build(
    content::BrowserContext* context) {
  scoped_ptr<SigninManagerBase> manager;
  Profile* profile = static_cast<Profile*>(context);
#if defined(OS_CHROMEOS)
  manager.reset(new FakeSigninManagerBase(profile));
#else
  manager.reset(new FakeSigninManager(profile));
#endif
  manager->Initialize(NULL);
  SigninManagerFactory::GetInstance()
      ->NotifyObserversOfSigninManagerCreationForTesting(manager.get());
  return manager.Pass();
}

#if !defined (OS_CHROMEOS)

FakeSigninManager::FakeSigninManager(Profile* profile)
    : SigninManager(
          ChromeSigninClientFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          AccountTrackerServiceFactory::GetForProfile(profile),
          GaiaCookieManagerServiceFactory::GetForProfile(profile)) {}

FakeSigninManager::~FakeSigninManager() {
}

void FakeSigninManager::StartSignInWithRefreshToken(
    const std::string& refresh_token,
    const std::string& gaia_id,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& oauth_fetched_callback) {
  set_auth_in_progress(
      account_tracker_service()->SeedAccountInfo(gaia_id, username));
  set_password(password);
  username_ = username;

  if (!oauth_fetched_callback.is_null())
    oauth_fetched_callback.Run(refresh_token);
}


void FakeSigninManager::CompletePendingSignin() {
  SetAuthenticatedAccountId(GetAccountIdForAuthInProgress());
  set_auth_in_progress(std::string());
  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSigninSucceeded(authenticated_account_id_,
                                          username_,
                                          password_));
}

void FakeSigninManager::SignIn(const std::string& gaia_id,
                               const std::string& username,
                               const std::string& password) {
  StartSignInWithRefreshToken(
      std::string(), gaia_id, username, password,
      OAuthTokenFetchedCallback());
  CompletePendingSignin();
}

void FakeSigninManager::FailSignin(const GoogleServiceAuthError& error) {
  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSigninFailed(error));
}

void FakeSigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric) {
  if (IsSignoutProhibited())
    return;
  set_auth_in_progress(std::string());
  set_password(std::string());
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = GetAuthenticatedUsername();
  authenticated_account_id_.clear();

  FOR_EACH_OBSERVER(SigninManagerBase::Observer, observer_list_,
                    GoogleSignedOut(account_id, username));
}

#endif  // !defined (OS_CHROMEOS)
