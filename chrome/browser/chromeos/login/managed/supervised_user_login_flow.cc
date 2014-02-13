// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/supervised_user_login_flow.h"

#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

SupervisedUserLoginFlow::SupervisedUserLoginFlow(
    const std::string& user_id)
    : ExtendedUserFlow(user_id),
      data_loaded_(false),
      weak_factory_(this) {
}

SupervisedUserLoginFlow::~SupervisedUserLoginFlow() {}

bool SupervisedUserLoginFlow::CanLockScreen() {
  return true;
}

bool SupervisedUserLoginFlow::ShouldLaunchBrowser() {
  return data_loaded_;
}

bool SupervisedUserLoginFlow::ShouldSkipPostLoginScreens() {
  return true;
}

bool SupervisedUserLoginFlow::HandleLoginFailure(
    const LoginFailure& failure) {
  return false;
}

bool SupervisedUserLoginFlow::HandlePasswordChangeDetected() {
  return false;
}

void SupervisedUserLoginFlow::HandleOAuthTokenStatusChange(
    User::OAuthTokenStatus status) {
}

void SupervisedUserLoginFlow::OnSyncSetupDataLoaded(
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ConfigureSync(token);
}

void SupervisedUserLoginFlow::ConfigureSync(const std::string& token) {
  data_loaded_ = true;

  // TODO(antrim): add error handling (no token loaded).
  // See also: http://crbug.com/312751
  UserManager::Get()->GetSupervisedUserManager()->ConfigureSyncWithToken(
      profile_, token);

  LoginUtils::Get()->DoBrowserLaunch(profile_, host());
  profile_ = NULL;
  UnregisterFlowSoon();
}

void SupervisedUserLoginFlow::LaunchExtraSteps(
    Profile* profile) {
  profile_ = profile;
  UserManager::Get()->GetSupervisedUserManager()->LoadSupervisedUserToken(
      profile,
      base::Bind(
           &SupervisedUserLoginFlow::OnSyncSetupDataLoaded,
           weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
