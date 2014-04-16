// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker_factory.h"

#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/common/profile_management_switches.h"

SigninTrackerFactory::SigninTrackerFactory() {}
SigninTrackerFactory::~SigninTrackerFactory() {}

// static
scoped_ptr<SigninTracker> SigninTrackerFactory::CreateForProfile(
    Profile* profile,
    SigninTracker::Observer* observer) {
  // Determine whether to use the AccountReconcilor.
  AccountReconcilor* account_reconcilor = NULL;
  if (!switches::IsEnableWebBasedSignin() && switches::IsNewProfileManagement())
    account_reconcilor = AccountReconcilorFactory::GetForProfile(profile);

  return scoped_ptr<SigninTracker>(new SigninTracker(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      account_reconcilor,
      ChromeSigninClientFactory::GetForProfile(profile),
      observer));
}
