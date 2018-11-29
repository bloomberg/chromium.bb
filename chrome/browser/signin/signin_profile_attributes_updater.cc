// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/core/browser/account_info.h"

SigninProfileAttributesUpdater::SigninProfileAttributesUpdater(
    SigninManagerBase* signin_manager,
    SigninErrorController* signin_error_controller,
    const base::FilePath& profile_path)
    : signin_error_controller_(signin_error_controller),
      profile_path_(profile_path),
      signin_error_controller_observer_(this),
      signin_manager_observer_(this) {
  signin_error_controller_observer_.Add(signin_error_controller);
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. Profile
  // metrics depend on this bug and must be fixed first.
}

SigninProfileAttributesUpdater::~SigninProfileAttributesUpdater() = default;

void SigninProfileAttributesUpdater::Shutdown() {
  signin_error_controller_observer_.RemoveAll();
  signin_manager_observer_.RemoveAll();
}

void SigninProfileAttributesUpdater::OnErrorChanged() {
  // Some tests don't have a ProfileManager.
  if (g_browser_process->profile_manager() == nullptr)
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_path_, &entry)) {
    return;
  }

  entry->SetIsAuthError(signin_error_controller_->HasError());
}

#if !defined(OS_CHROMEOS)
void SigninProfileAttributesUpdater::GoogleSigninSucceeded(
    const AccountInfo& account_info) {
  ProfileAttributesEntry* entry;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_path_, &entry)) {
    return;
  }

  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(account_info.email));
  ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
}

void SigninProfileAttributesUpdater::GoogleSignedOut(
    const AccountInfo& account_info) {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()
                       ->GetProfileAttributesStorage()
                       .GetProfileAttributesWithPath(profile_path_, &entry);

  // If sign out occurs because Sync setup was in progress and the Profile got
  // deleted, then the profile's no longer in the ProfileAttributesStorage.
  if (!has_entry)
    return;

  entry->SetLocalAuthCredentials(std::string());
  entry->SetAuthInfo(std::string(), base::string16());
  entry->SetIsSigninRequired(false);
}
#endif  // !defined(OS_CHROMEOS)
