// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_play_store_enabled_preference_handler.h"

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

ArcPlayStoreEnabledPreferenceHandler::ArcPlayStoreEnabledPreferenceHandler(
    Profile* profile,
    ArcSessionManager* arc_session_manager)
    : profile_(profile),
      arc_session_manager_(arc_session_manager),
      weak_ptr_factory_(this) {
  DCHECK(profile_);
  DCHECK(arc_session_manager_);
}

ArcPlayStoreEnabledPreferenceHandler::~ArcPlayStoreEnabledPreferenceHandler() {
  ArcAuthNotification::Hide();
  sync_preferences::PrefServiceSyncable* pref_service_syncable =
      PrefServiceSyncableFromProfile(profile_);
  pref_service_syncable->RemoveObserver(this);
  pref_change_registrar_.RemoveAll();
}

void ArcPlayStoreEnabledPreferenceHandler::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Start observing Google Play Store enabled preference.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcEnabled,
      base::Bind(&ArcPlayStoreEnabledPreferenceHandler::OnPreferenceChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  const bool is_play_store_enabled = IsArcPlayStoreEnabledForProfile(profile_);
  VLOG(1) << "Start observing Google Play Store enabled preference. "
          << "Initial value: " << is_play_store_enabled;
  UpdateArcSessionManager();

  if (is_play_store_enabled)
    return;

  // Google Play Store is initially disabled, here.

  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_)) {
    // All users that can disable Google Play Store by themselves will have
    // the |kARcDataRemoveRequested| pref set, so we don't need to eagerly
    // remove the data for that case.
    // For managed users, the preference can change when the Profile object is
    // not alive, so we still need to check it here in case it was disabled to
    // ensure that the data is deleted in case it was disabled between
    // launches.
    VLOG(1) << "Google Play Store is initially disabled for managed "
            << "profile. Removing data.";
    arc_session_manager_->RemoveArcData();
  }

  // ArcAuthNotification may need to be shown.
  PrefServiceSyncableFromProfile(profile_)->AddObserver(this);
  OnIsSyncingChanged();
}

void ArcPlayStoreEnabledPreferenceHandler::OnPreferenceChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool is_play_store_enabled = IsArcPlayStoreEnabledForProfile(profile_);
  if (!IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_)) {
    // Update UMA only for non-Managed cases.
    UpdateOptInActionUMA(is_play_store_enabled ? OptInActionType::OPTED_IN
                                               : OptInActionType::OPTED_OUT);

    if (!is_play_store_enabled) {
      // Remove the pinned Play Store icon launcher in Shelf.
      // This is only for non-Managed cases. In managed cases, it is expected
      // to be "disabled" rather than "removed", so keep it here.
      auto* shelf_delegate = ash::WmShell::HasInstance()
                                 ? ash::WmShell::Get()->shelf_delegate()
                                 : nullptr;
      if (shelf_delegate)
        shelf_delegate->UnpinAppWithID(ArcSupportHost::kHostAppId);
    }
  }

  // Hide auth notification if it was opened before and arc.enabled pref was
  // explicitly set to true or false.
  if (profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled))
    ArcAuthNotification::Hide();

  UpdateArcSessionManager();

  // Due to life time management, OnArcPlayStoreEnabledChanged() is notified
  // via ArcSessionManager, so that external services can subscribe at almost
  // any time.
  arc_session_manager_->NotifyArcPlayStoreEnabledChanged(is_play_store_enabled);
}

void ArcPlayStoreEnabledPreferenceHandler::UpdateArcSessionManager() {
  auto* support_host = arc_session_manager_->support_host();
  if (support_host) {
    support_host->SetArcManaged(
        IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_));
  }

  if (IsArcPlayStoreEnabledForProfile(profile_))
    arc_session_manager_->RequestEnable();
  else
    arc_session_manager_->RequestDisable();
}

void ArcPlayStoreEnabledPreferenceHandler::OnIsSyncingChanged() {
  sync_preferences::PrefServiceSyncable* const pref_service_syncable =
      PrefServiceSyncableFromProfile(profile_);
  if (!pref_service_syncable->IsSyncing())
    return;
  pref_service_syncable->RemoveObserver(this);

  // TODO(hidehiko): Extract kEnableArcOOBEOptIn check as a utility method.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableArcOOBEOptIn) &&
      profile_->IsNewProfile() &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kArcEnabled)) {
    ArcAuthNotification::Show(profile_);
  }
}

}  // namespace arc
