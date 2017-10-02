// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class Profile;

namespace arc {

class ArcSessionManager;
class ArcAuthNotification;

// Observes Google Play Store enabled preference (whose key is "arc.enabled"
// for historical reason), and controls ARC via ArcSessionManager.
// In addition, this controls showing/hiding a notification to enable Google
// Play Store.
class ArcPlayStoreEnabledPreferenceHandler
    : public sync_preferences::PrefServiceSyncableObserver {
 public:
  ArcPlayStoreEnabledPreferenceHandler(Profile* profile,
                                       ArcSessionManager* arc_session_manager);
  ~ArcPlayStoreEnabledPreferenceHandler() override;

  // Starts observing Google Play Store enabled preference change.
  // Also, based on its initial value, this may start ArcSession, or may start
  // removing the data, as initial state.
  // In addition, this triggers to show ArcAuthNotification, if necessary.
  void Start();

 private:
  // Called when Google Play Store enabled preference is changed.
  void OnPreferenceChanged();

  // Updates enabling/disabling of ArcSessionManager. Also, sets
  // ArcSupportHost's managed state.
  void UpdateArcSessionManager();

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  Profile* const profile_;

  // Owned by ArcServiceLauncher.
  ArcSessionManager* const arc_session_manager_;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  // Controls life-cycle of ARC auth notification.
  std::unique_ptr<ArcAuthNotification> auth_notification_;

  // Must be the last member.
  base::WeakPtrFactory<ArcPlayStoreEnabledPreferenceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreEnabledPreferenceHandler);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_PLAY_STORE_ENABLED_PREFERENCE_HANDLER_H_
