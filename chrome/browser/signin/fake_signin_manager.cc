// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_signin_manager.h"

#include "base/callback_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"

FakeSigninManagerBase::FakeSigninManagerBase(Profile* profile)
    : SigninManagerBase(
          ChromeSigninClientFactory::GetInstance()->GetForProfile(profile)) {}

FakeSigninManagerBase::~FakeSigninManagerBase() {
}

// static
KeyedService* FakeSigninManagerBase::Build(content::BrowserContext* context) {
  SigninManagerBase* manager;
  Profile* profile = static_cast<Profile*>(context);
#if defined(OS_CHROMEOS)
  manager = new FakeSigninManagerBase(profile);
#else
  manager = new FakeSigninManager(profile);
#endif
  manager->Initialize(NULL);
  SigninManagerFactory::GetInstance()
      ->NotifyObserversOfSigninManagerCreationForTesting(manager);
  return manager;
}

#if !defined (OS_CHROMEOS)

FakeSigninManager::FakeSigninManager(Profile* profile)
    : SigninManager(
          ChromeSigninClientFactory::GetInstance()->GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile)) {}

FakeSigninManager::~FakeSigninManager() {
}

void FakeSigninManager::StartSignInWithRefreshToken(
    const std::string& refresh_token,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& oauth_fetched_callback) {
  set_auth_in_progress(username);
  set_password(password);
  if (!oauth_fetched_callback.is_null())
    oauth_fetched_callback.Run(refresh_token);
}


void FakeSigninManager::CompletePendingSignin() {
  SetAuthenticatedUsername(GetUsernameForAuthInProgress());
  set_auth_in_progress(std::string());
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    GoogleSigninSucceeded(authenticated_username_, password_));
}

void FakeSigninManager::AddMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  SigninManager::AddMergeSessionObserver(observer);
  merge_session_observer_list_.AddObserver(observer);
}

void FakeSigninManager::RemoveMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  SigninManager::RemoveMergeSessionObserver(observer);
  merge_session_observer_list_.RemoveObserver(observer);
}

void FakeSigninManager::NotifyMergeSessionObservers(
    const GoogleServiceAuthError& error) {
  FOR_EACH_OBSERVER(MergeSessionHelper::Observer, merge_session_observer_list_,
                    MergeSessionCompleted(GetAuthenticatedUsername(), error));
}

void FakeSigninManager::SignIn(const std::string& username,
                               const std::string& password) {
  StartSignInWithRefreshToken(
      std::string(), username, password, OAuthTokenFetchedCallback());
  CompletePendingSignin();
}

void FakeSigninManager::FailSignin(const GoogleServiceAuthError& error) {
  FOR_EACH_OBSERVER(Observer, observer_list_, GoogleSigninFailed(error));
}

void FakeSigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric) {
  if (IsSignoutProhibited())
    return;
  set_auth_in_progress(std::string());
  set_password(std::string());
  const std::string username = authenticated_username_;
  authenticated_username_.clear();

  FOR_EACH_OBSERVER(SigninManagerBase::Observer, observer_list_,
                    GoogleSignedOut(username));
}

#endif  // !defined (OS_CHROMEOS)
