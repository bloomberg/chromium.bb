// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace {

constexpr const char kUserActionConinueAndReview[] = "continue-and-review";
constexpr const char kUserActionContinueWithDefaults[] =
    "continue-with-defaults";

browser_sync::ProfileSyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

}  // namespace

SyncConsentScreen::SyncConsentScreen(BaseScreenDelegate* base_screen_delegate,
                                     SyncConsentScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_SYNC_CONSENT),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

SyncConsentScreen::~SyncConsentScreen() {
  view_->Bind(NULL);
}

void SyncConsentScreen::Show() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  profile_ = ProfileHelper::Get()->GetProfileByUser(user);

  // Populate initial value.
  view_->OnUserPrefKnown(true, GetSyncService(profile_)->IsManaged());

  // Show the screen.
  view_->Show();
}

void SyncConsentScreen::Hide() {
  view_->Hide();
}

void SyncConsentScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionConinueAndReview) {
    profile_->GetPrefs()->SetBoolean(prefs::kShowSyncSettingsOnSessionStart,
                                     true);
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }
  if (action_id == kUserActionContinueWithDefaults) {
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

void SyncConsentScreen::SetSyncAllValue(bool sync_all) {
  browser_sync::ProfileSyncService* service = GetSyncService(profile_);
  if (!service->IsManaged()) {
    // When |sync_all| is true, second parameter is ignored.
    // When it's false, second set defines individual data types to be synced.
    // We want none, so empty set does what we need.
    service->OnUserChoseDatatypes(sync_all, syncer::ModelTypeSet());
  }
}

}  // namespace chromeos
