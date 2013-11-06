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
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

std::string LoadSyncToken() {
  std::string token;
  base::FilePath token_file =
      file_util::GetHomeDir().Append(kManagedUserTokenFilename);
  if (!base::ReadFileToString(token_file, &token)) {
    return std::string();
  }
  return token;
}

} // namespace

LocallyManagedUserLoginFlow::LocallyManagedUserLoginFlow(
    const std::string& user_id)
    : ExtendedUserFlow(user_id),
      data_loaded_(false),
      weak_factory_(this) {
}

LocallyManagedUserLoginFlow::~LocallyManagedUserLoginFlow() {}

bool LocallyManagedUserLoginFlow::ShouldLaunchBrowser() {
  return data_loaded_;
}

bool LocallyManagedUserLoginFlow::ShouldSkipPostLoginScreens() {
  return true;
}

bool LocallyManagedUserLoginFlow::HandleLoginFailure(
    const LoginFailure& failure) {
  return false;
}

bool LocallyManagedUserLoginFlow::HandlePasswordChangeDetected() {
  return false;
}

void LocallyManagedUserLoginFlow::HandleOAuthTokenStatusChange(
    User::OAuthTokenStatus status) {
}

void LocallyManagedUserLoginFlow::OnSyncSetupDataLoaded(
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ConfigureSync(token);
}

void LocallyManagedUserLoginFlow::ConfigureSync(const std::string& token) {
  data_loaded_ = true;
  // TODO(antrim): add error handling (no token loaded).
  // See also: http://crbug.com/312751
  if (!token.empty())
    ManagedUserServiceFactory::GetForProfile(profile_)->InitSync(token);

  LoginUtils::Get()->DoBrowserLaunch(profile_, host());
  profile_ = NULL;
  UnregisterFlowSoon();
}

void LocallyManagedUserLoginFlow::LaunchExtraSteps(
    Profile* profile) {
  profile_ = profile;
  const std::string token;
  if (token.empty()) {
    PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&LoadSyncToken),
        base::Bind(
             &LocallyManagedUserLoginFlow::OnSyncSetupDataLoaded,
             weak_factory_.GetWeakPtr()));
  } else {
    ConfigureSync(token);
  }
}

}  // namespace chromeos
