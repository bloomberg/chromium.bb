// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_
#define CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

class DBusThreadManager;

// PowerPolicyController is responsible for sending Chrome's assorted power
// management preferences to the Chrome OS power manager.
class CHROMEOS_EXPORT PowerPolicyController
    : public DBusThreadManagerObserver,
      public PowerManagerClient::Observer {
 public:
  // Note: Do not change these values; they are used by preferences.
  enum Action {
    ACTION_SUSPEND      = 0,
    ACTION_STOP_SESSION = 1,
    ACTION_SHUT_DOWN    = 2,
    ACTION_DO_NOTHING   = 3,
  };

  PowerPolicyController(DBusThreadManager* manager, PowerManagerClient* client);
  virtual ~PowerPolicyController();

  // Sends an updated policy to the power manager based on the current
  // values of the passed-in prefs.
  void UpdatePolicyFromPrefs(
      const PrefService::Preference& ac_screen_dim_delay_ms_pref,
      const PrefService::Preference& ac_screen_off_delay_ms_pref,
      const PrefService::Preference& ac_screen_lock_delay_ms_pref,
      const PrefService::Preference& ac_idle_warning_delay_ms_pref,
      const PrefService::Preference& ac_idle_delay_ms_pref,
      const PrefService::Preference& battery_screen_dim_delay_ms_pref,
      const PrefService::Preference& battery_screen_off_delay_ms_pref,
      const PrefService::Preference& battery_screen_lock_delay_ms_pref,
      const PrefService::Preference& battery_idle_warning_delay_ms_pref,
      const PrefService::Preference& battery_idle_delay_ms_pref,
      const PrefService::Preference& idle_action_pref,
      const PrefService::Preference& lid_closed_action_pref,
      const PrefService::Preference& use_audio_activity_pref,
      const PrefService::Preference& use_video_activity_pref,
      const PrefService::Preference& presentation_idle_delay_factor_pref);

  // DBusThreadManagerObserver implementation:
  virtual void OnDBusThreadManagerDestroying(DBusThreadManager* manager)
      OVERRIDE;

  // PowerManagerClient::Observer implementation:
  virtual void PowerManagerRestarted() OVERRIDE;

 private:
  // Sends a policy based on |prefs_policy_| to the power manager.
  void SendCurrentPolicy();

  // Sends an empty policy to the power manager to reset its configuration.
  void SendEmptyPolicy();

  DBusThreadManager* manager_;  // not owned
  PowerManagerClient* client_;  // not owned

  // Policy specified by the prefs that were last passed to
  // UpdatePolicyFromPrefs().
  power_manager::PowerManagementPolicy prefs_policy_;

  DISALLOW_COPY_AND_ASSIGN(PowerPolicyController);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_POLICY_CONTROLLER_H_
