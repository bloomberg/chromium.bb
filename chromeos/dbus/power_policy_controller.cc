// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_policy_controller.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

// If |pref|, a PrefService::Preference containing an integer, has been
// explicitly set to 0 or a positive value, assigns it to |proto_field|, a
// int32 field in |proto|, a google::protobuf::MessageLite*.
#define SET_DELAY_FROM_PREF(pref, proto_field, proto)                          \
    {                                                                          \
      int value = GetIntPrefValue(pref);                                       \
      if (value >= 0)                                                          \
        (proto)->set_##proto_field(value);                                     \
    }

// Similar to SET_DELAY_FROM_PREF() but sets a
// power_manager::PowerManagementPolicy_Action field instead.
#define SET_ACTION_FROM_PREF(pref, proto_field, proto)                         \
    {                                                                          \
      int value = GetIntPrefValue(pref);                                       \
      if (value >= 0) {                                                        \
        (proto)->set_##proto_field(                                            \
            static_cast<power_manager::PowerManagementPolicy_Action>(value));  \
      }                                                                        \
    }

// If |pref|, a PrefService::Preference containing a bool, has been
// set, assigns it to |proto_field|, a bool field in |proto|, a
// google::protobuf::MessageLite*.
#define SET_BOOL_FROM_PREF(pref, proto_field, proto)                           \
    if (!pref.IsDefaultValue()) {                                              \
      bool value = false;                                                      \
      if (pref.GetValue()->GetAsBoolean(&value))                               \
        (proto)->set_##proto_field(value);                                     \
      else                                                                     \
        LOG(DFATAL) << pref.name() << " pref has non-boolean value";           \
    }

// Returns a zero or positive integer value from |pref|.  Returns -1 if the
// pref is unset and logs an error if it's set to a negative value.
int GetIntPrefValue(const PrefService::Preference& pref) {
  if (pref.IsDefaultValue())
    return -1;

  int value = -1;
  if (!pref.GetValue()->GetAsInteger(&value)) {
    LOG(DFATAL) << pref.name() << " pref has non-integer value";
    return -1;
  }

  if (value < 0)
    LOG(WARNING) << pref.name() << " pref has negative value";
  return value;
}

// Returns the power_manager::PowerManagementPolicy_Action value
// corresponding to |action|.
power_manager::PowerManagementPolicy_Action GetProtoAction(
    PowerPolicyController::Action action) {
  switch (action) {
    case PowerPolicyController::ACTION_SUSPEND:
      return power_manager::PowerManagementPolicy_Action_SUSPEND;
    case PowerPolicyController::ACTION_STOP_SESSION:
      return power_manager::PowerManagementPolicy_Action_STOP_SESSION;
    case PowerPolicyController::ACTION_SHUT_DOWN:
      return power_manager::PowerManagementPolicy_Action_SHUT_DOWN;
    case PowerPolicyController::ACTION_DO_NOTHING:
      return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
    default:
      NOTREACHED() << "Unhandled action " << action;
      return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
  }
}

}  // namespace

PowerPolicyController::PowerPolicyController(DBusThreadManager* manager,
                                             PowerManagerClient* client)
    : manager_(manager),
      client_(client) {
  manager_->AddObserver(this);
  client_->AddObserver(this);
  SendEmptyPolicy();
}

PowerPolicyController::~PowerPolicyController() {
  // The power manager's policy is reset before this point, in
  // OnDBusThreadManagerDestroying().  At the time that
  // PowerPolicyController is destroyed, PowerManagerClient's D-Bus proxy
  // to the power manager is already gone.
  client_->RemoveObserver(this);
  client_ = NULL;
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void PowerPolicyController::UpdatePolicyFromPrefs(
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
    const PrefService::Preference& presentation_idle_delay_factor_pref) {
  prefs_policy_.Clear();

  power_manager::PowerManagementPolicy::Delays* delays =
      prefs_policy_.mutable_ac_delays();
  SET_DELAY_FROM_PREF(ac_screen_dim_delay_ms_pref, screen_dim_ms, delays);
  SET_DELAY_FROM_PREF(ac_screen_off_delay_ms_pref, screen_off_ms, delays);
  SET_DELAY_FROM_PREF(ac_screen_lock_delay_ms_pref, screen_lock_ms, delays);
  SET_DELAY_FROM_PREF(ac_idle_warning_delay_ms_pref, idle_warning_ms, delays);
  SET_DELAY_FROM_PREF(ac_idle_delay_ms_pref, idle_ms, delays);

  delays = prefs_policy_.mutable_battery_delays();
  SET_DELAY_FROM_PREF(battery_screen_dim_delay_ms_pref, screen_dim_ms, delays);
  SET_DELAY_FROM_PREF(battery_screen_off_delay_ms_pref, screen_off_ms, delays);
  SET_DELAY_FROM_PREF(
      battery_screen_lock_delay_ms_pref, screen_lock_ms, delays);
  SET_DELAY_FROM_PREF(
      battery_idle_warning_delay_ms_pref, idle_warning_ms, delays);
  SET_DELAY_FROM_PREF(battery_idle_delay_ms_pref, idle_ms, delays);

  SET_ACTION_FROM_PREF(idle_action_pref, idle_action, &prefs_policy_);
  SET_ACTION_FROM_PREF(
      lid_closed_action_pref, lid_closed_action, &prefs_policy_);

  SET_BOOL_FROM_PREF(
      use_audio_activity_pref, use_audio_activity, &prefs_policy_);
  SET_BOOL_FROM_PREF(
      use_video_activity_pref, use_video_activity, &prefs_policy_);

  if (!presentation_idle_delay_factor_pref.IsDefaultValue()) {
    double value = 0.0;
    if (presentation_idle_delay_factor_pref.GetValue()->GetAsDouble(&value)) {
      prefs_policy_.set_presentation_idle_delay_factor(value);
    } else {
      LOG(DFATAL) << presentation_idle_delay_factor_pref.name()
                  << " pref has non-double value";
    }
  }

  SendCurrentPolicy();
}

void PowerPolicyController::OnDBusThreadManagerDestroying(
    DBusThreadManager* manager) {
  DCHECK_EQ(manager, manager_);
  SendEmptyPolicy();
}

void PowerPolicyController::PowerManagerRestarted() {
  SendCurrentPolicy();
}

void PowerPolicyController::SendCurrentPolicy() {
  // TODO(derat): Incorporate other information into the policy that is
  // sent, e.g. from content::PowerSaveBlocker.
  client_->SetPolicy(prefs_policy_);
}

void PowerPolicyController::SendEmptyPolicy() {
  client_->SetPolicy(power_manager::PowerManagementPolicy());
}

}  // namespace chromeos
