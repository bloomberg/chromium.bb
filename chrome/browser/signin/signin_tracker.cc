// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/profile_management_switches.h"
#include "components/signin/core/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager.h"
#endif

SigninTracker::SigninTracker(Profile* profile, Observer* observer)
    : profile_(profile), observer_(observer) {
  DCHECK(profile_);
  Initialize();
}

SigninTracker::~SigninTracker() {
  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      RemoveObserver(this);

  if (!switches::IsEnableWebBasedSignin()) {
    if (switches::IsNewProfileManagement()) {
        AccountReconcilorFactory::GetForProfile(profile_)->
            RemoveMergeSessionObserver(this);
#if !defined(OS_CHROMEOS)
    } else {
        SigninManagerFactory::GetForProfile(profile_)->
            RemoveMergeSessionObserver(this);
#endif
    }
  }
}

void SigninTracker::Initialize() {
  DCHECK(observer_);

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->AddObserver(this);
  OAuth2TokenService* token_service =
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->AddObserver(this);
}

void SigninTracker::GoogleSigninFailed(const GoogleServiceAuthError& error) {
  observer_->SigninFailed(error);
}

void SigninTracker::OnRefreshTokenAvailable(const std::string& account_id) {
  // TODO: when OAuth2TokenService handles multi-login, this should check
  // that |account_id| is the primary account before signalling success.
  if (!switches::IsEnableWebBasedSignin()) {
    if (switches::IsNewProfileManagement()) {
      AccountReconcilorFactory::GetForProfile(profile_)->
          AddMergeSessionObserver(this);
#if !defined(OS_CHROMEOS)
    } else {
      SigninManagerFactory::GetForProfile(profile_)->
          AddMergeSessionObserver(this);
#endif
    }
  }

  observer_->SigninSuccess();
}

void SigninTracker::OnRefreshTokenRevoked(const std::string& account_id) {
  NOTREACHED();
}

void SigninTracker::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer_->MergeSessionComplete(error);
}
