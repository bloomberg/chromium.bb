// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_login_flow.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

// namespace {
//
//  A pref for the sync token.
// const char kSyncServiceAuthorizationToken[] =
//    "ManagedUserSyncServiceAuthorizationToken";
//
// } // namespace
//
// // static
// void LocallyManagedUserLoginFlow::RegisterPrefs(
//    PrefRegistrySimple* registry) {
//   registry->RegisterStringPref(
//       kSyncServiceAuthorizationToken, "");
// }

LocallyManagedUserLoginFlow::LocallyManagedUserLoginFlow(
    const std::string& user_id)
    : ExtendedUserFlow(user_id),
      data_loaded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

LocallyManagedUserLoginFlow::~LocallyManagedUserLoginFlow() {}

bool LocallyManagedUserLoginFlow::ShouldLaunchBrowser() {
  return data_loaded_;
}

bool LocallyManagedUserLoginFlow::ShouldSkipPostLoginScreens() {
  return false;
}

bool LocallyManagedUserLoginFlow::HandleLoginFailure(
    const LoginFailure& failure,
    LoginDisplayHost* host) {
  return false;
}

bool LocallyManagedUserLoginFlow::HandlePasswordChangeDetected(
    LoginDisplayHost* host) {
  return false;
}

void LocallyManagedUserLoginFlow::LoadSyncSetupData() {
  std::string token;
  base::FilePath token_file = file_util::GetHomeDir().Append("token");
  file_util::ReadFileToString(token_file, &token);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocallyManagedUserLoginFlow::OnSyncSetupDataLoaded,
           weak_factory_.GetWeakPtr(),
           token));
}

void LocallyManagedUserLoginFlow::OnSyncSetupDataLoaded(
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

//  PrefService* prefs = profile_->GetPrefs();
//  prefs->SetString(kSyncServiceAuthorizationToken,
//      token);
  ConfigureSync(token);
}

void LocallyManagedUserLoginFlow::ConfigureSync(const std::string& token) {
  data_loaded_ = true;
  // TODO(antrim): Propagate token when we know API.

  LoginUtils::Get()->DoBrowserLaunch(profile_, host_);
  profile_ = NULL;
  host_ = NULL;
  UnregisterFlowSoon();
}

void LocallyManagedUserLoginFlow::LaunchExtraSteps(
    Profile* profile,
    LoginDisplayHost* host) {
  profile_ = profile;
  host_ = host;
//  PrefService* prefs = profile->GetPrefs();
  const std::string token;
//     =  prefs->GetString(kSyncServiceAuthorizationToken);
  if (token.empty()) {
    BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&LocallyManagedUserLoginFlow::LoadSyncSetupData,
             weak_factory_.GetWeakPtr()));
  } else {
    ConfigureSync(token);
  }
}

}  // namespace chromeos
