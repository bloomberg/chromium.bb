// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

PaletteDelegateChromeOS::PaletteDelegateChromeOS() : weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

PaletteDelegateChromeOS::~PaletteDelegateChromeOS() {}

std::unique_ptr<PaletteDelegateChromeOS::EnableListenerSubscription>
PaletteDelegateChromeOS::AddPaletteEnableListener(
    const EnableListener& on_state_changed) {
  auto subscription = palette_enabled_callback_list_.Add(on_state_changed);
  OnPaletteEnabledPrefChanged();
  return subscription;
}

void PaletteDelegateChromeOS::ActiveUserChanged(
    const user_manager::User* active_user) {
  SetProfile(ProfileHelper::Get()->GetProfileByUser(active_user));
}

void PaletteDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetActiveUserProfile());

      // Add a session state observer to be able to monitor session changes.
      if (!session_state_observer_.get()) {
        session_state_observer_.reset(
            new user_manager::ScopedUserSessionStateObserver(this));
      }
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Update |profile_| when exiting a session or shutting down.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_ == profile)
        SetProfile(nullptr);
      break;
    }
  }
}

void PaletteDelegateChromeOS::OnPaletteEnabledPrefChanged() {
  if (profile_) {
    palette_enabled_callback_list_.Notify(
        profile_->GetPrefs()->GetBoolean(prefs::kEnableStylusTools));
  }
}

void PaletteDelegateChromeOS::SetProfile(Profile* profile) {
  profile_ = profile;
  pref_change_registrar_.reset();
  if (!profile_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kEnableStylusTools,
      base::Bind(&PaletteDelegateChromeOS::OnPaletteEnabledPrefChanged,
                 base::Unretained(this)));

  // Run listener with new pref value, if any.
  OnPaletteEnabledPrefChanged();
}

bool PaletteDelegateChromeOS::ShouldAutoOpenPalette() {
  if (!profile_)
    return false;

  return profile_->GetPrefs()->GetBoolean(prefs::kLaunchPaletteOnEjectEvent);
}

bool PaletteDelegateChromeOS::ShouldShowPalette() {
  if (!profile_)
    return false;

  return profile_->GetPrefs()->GetBoolean(prefs::kEnableStylusTools);
}

}  // namespace chromeos
