// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_signin_manager.h"

#include "base/callback_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_manager_delegate.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

FakeSigninManagerBase::FakeSigninManagerBase(Profile* profile) {
  profile_ = profile;
  signin_global_error_.reset(new SigninGlobalError(this, profile));
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
      signin_global_error_.get());
  signin_allowed_.Init(prefs::kSigninAllowed, profile_->GetPrefs(),
      base::Bind(&SigninManagerBase::OnSigninAllowedPrefChanged,
      base::Unretained(this)));
}

FakeSigninManagerBase::~FakeSigninManagerBase() {
  if (signin_global_error_.get()) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
        signin_global_error_.get());
    signin_global_error_.reset();
  }
}

void FakeSigninManagerBase::SignOut() {
  authenticated_username_.clear();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

// static
ProfileKeyedService* FakeSigninManagerBase::Build(
    content::BrowserContext* profile) {
  return new FakeSigninManagerBase(static_cast<Profile*>(profile));
}

#if !defined (OS_CHROMEOS)

FakeSigninManager::FakeSigninManager(Profile* profile)
    : SigninManager(scoped_ptr<SigninManagerDelegate>(
        new ChromeSigninManagerDelegate(profile))) {
  Initialize(profile);
}

FakeSigninManager::~FakeSigninManager() {
}

void FakeSigninManager::InitTokenService() {
}

void FakeSigninManager::StartSignIn(const std::string& username,
                                    const std::string& password,
                                    const std::string& login_token,
                                    const std::string& login_captcha) {
  SetAuthenticatedUsername(username);
}

void FakeSigninManager::StartSignInWithCredentials(
    const std::string& session_index,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& oauth_fetched_callback) {
  set_auth_in_progress(username);
  if (!oauth_fetched_callback.is_null())
    oauth_fetched_callback.Run("fake_oauth_token");
}

void FakeSigninManager::CompletePendingSignin() {
  SetAuthenticatedUsername(GetUsernameForAuthInProgress());
  set_auth_in_progress(std::string());
}

void FakeSigninManager::SignOut() {
  if (IsSignoutProhibited())
    return;
  set_auth_in_progress(std::string());
  authenticated_username_.clear();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

// static
ProfileKeyedService* FakeSigninManager::Build(
    content::BrowserContext* profile) {
  return new FakeSigninManager(static_cast<Profile*>(profile));
}

#endif  // !defined (OS_CHROMEOS)
