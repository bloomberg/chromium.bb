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
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/core/browser/account_info.h"

SigninProfileAttributesUpdater::SigninProfileAttributesUpdater(
    identity::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    const base::FilePath& profile_path)
    : identity_manager_(identity_manager),
      signin_error_controller_(signin_error_controller),
      profile_path_(profile_path),
      identity_manager_observer_(this),
      signin_error_controller_observer_(this) {
  // Some tests don't have a ProfileManager, disable this service.
  if (!g_browser_process->profile_manager())
    return;

  identity_manager_observer_.Add(identity_manager_);
  signin_error_controller_observer_.Add(signin_error_controller);

  UpdateProfileAttributes();
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. Profile
  // metrics depend on this bug and must be fixed first.
}

SigninProfileAttributesUpdater::~SigninProfileAttributesUpdater() = default;

void SigninProfileAttributesUpdater::Shutdown() {
  identity_manager_observer_.RemoveAll();
  signin_error_controller_observer_.RemoveAll();
}

void SigninProfileAttributesUpdater::UpdateProfileAttributes() {
  ProfileAttributesEntry* entry;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_path_, &entry)) {
    return;
  }

  std::string old_gaia_id = entry->GetGAIAId();

  if (identity_manager_->HasPrimaryAccount()) {
    CoreAccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
    entry->SetAuthInfo(account_info.gaia,
                       base::UTF8ToUTF16(account_info.email));
  } else {
    entry->SetLocalAuthCredentials(std::string());
    entry->SetAuthInfo(std::string(), base::string16());
    if (!signin_util::IsForceSigninEnabled())
      entry->SetIsSigninRequired(false);
  }

  if (old_gaia_id != entry->GetGAIAId())
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
}

void SigninProfileAttributesUpdater::OnErrorChanged() {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile_path_, &entry)) {
    return;
  }

  entry->SetIsAuthError(signin_error_controller_->HasError());
}

void SigninProfileAttributesUpdater::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdateProfileAttributes();
}

void SigninProfileAttributesUpdater::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdateProfileAttributes();
}
