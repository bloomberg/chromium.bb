// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace webui {
namespace {

void ShowUserManager(const ProfileManager::CreateCallback& callback) {
  if (!UserManager::IsShowing()) {
    UserManager::Show(base::FilePath(), profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  }

  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetSystemProfilePath(), callback, base::string16(),
      std::string(), std::string());
}

std::string GetProfileUserName(Profile* profile) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry))
    return std::string();
  return base::UTF16ToUTF8(entry->GetUserName());
}

void ShowSigninDialog(base::FilePath signin_profile_path,
                      Profile* system_profile,
                      Profile::CreateStatus status) {
  UserManagerProfileDialog::ShowSigninDialog(system_profile,
                                             signin_profile_path);
}

void ShowReauthDialog(const std::string& user_name,
                      Profile* system_profile,
                      Profile::CreateStatus status) {
  UserManagerProfileDialog::ShowReauthDialog(
      system_profile, user_name, signin_metrics::Reason::REASON_UNLOCK);
}

void DeleteProfileCallback(std::unique_ptr<ScopedKeepAlive> keep_alive,
                           Profile* profile,
                           Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  OpenNewWindowForProfile(profile);
}

}  // namespace

void OpenNewWindowForProfile(Profile* profile) {
  if (profiles::IsProfileLocked(profile->GetPath())) {
    if (signin::IsForceSigninEnabled()) {
      ShowUserManager(base::Bind(&ShowSigninDialog, profile->GetPath()));
    } else {
      ShowUserManager(
          base::Bind(&ShowReauthDialog, GetProfileUserName(profile)));
    }
  } else {
    profiles::FindOrCreateNewWindowForProfile(
        profile, chrome::startup::IS_PROCESS_STARTUP,
        chrome::startup::IS_FIRST_RUN, false);
  }
}

void DeleteProfileAtPath(base::FilePath file_path,
                         content::WebUI* web_ui,
                         ProfileMetrics::ProfileDelete deletion_source) {
  DCHECK(web_ui);

  if (!profiles::IsMultipleProfilesEnabled())
    return;
  g_browser_process->profile_manager()->MaybeScheduleProfileForDeletion(
      file_path, base::Bind(&DeleteProfileCallback,
                            base::Passed(base::MakeUnique<ScopedKeepAlive>(
                                KeepAliveOrigin::PROFILE_HELPER,
                                KeepAliveRestartOption::DISABLED))),
      deletion_source);
}

}  // namespace webui
