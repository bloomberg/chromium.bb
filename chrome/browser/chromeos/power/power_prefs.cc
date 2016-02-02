// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_prefs.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

PowerPrefs::PowerPrefs(PowerPolicyController* power_policy_controller)
    : power_policy_controller_(power_policy_controller),
      profile_(NULL),
      screen_is_locked_(false) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_SESSION_STARTED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
}

PowerPrefs::~PowerPrefs() {
}

// static
void PowerPrefs::RegisterUserProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);

  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleAction,
                                PowerPolicyController::ACTION_SUSPEND);
  registry->RegisterIntegerPref(prefs::kPowerLidClosedAction,
                                PowerPolicyController::ACTION_SUSPEND);
}

// static
void PowerPrefs::RegisterLoginProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry);

  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleAction,
                                PowerPolicyController::ACTION_SHUT_DOWN);
  registry->RegisterIntegerPref(prefs::kPowerLidClosedAction,
                                PowerPolicyController::ACTION_SHUT_DOWN);
}

void PowerPrefs::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      // Update |profile_| when entering the login screen.
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (!profile_manager || !profile_manager->IsLoggedIn())
        SetProfile(ProfileHelper::GetSigninProfile());
      break;
    }
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED:
      // Update the policy in case different delays have been set for the lock
      // screen.
      screen_is_locked_ = *content::Details<bool>(details).ptr();
      UpdatePowerPolicyFromPrefs();
      break;
    case chrome::NOTIFICATION_SESSION_STARTED:
      // Update |profile_| when entering a session.
      SetProfile(ProfileManager::GetPrimaryUserProfile());
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Update |profile_| when exiting a session or shutting down.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_ == profile)
        SetProfile(NULL);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void PowerPrefs::UpdatePowerPolicyFromPrefs() {
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    NOTREACHED();
    return;
  }

  const PrefService* prefs = pref_change_registrar_->prefs();
  PowerPolicyController::PrefValues values;
  values.ac_screen_dim_delay_ms =
      prefs->GetInteger(screen_is_locked_ ? prefs::kPowerLockScreenDimDelayMs :
          prefs::kPowerAcScreenDimDelayMs);
  values.ac_screen_off_delay_ms =
      prefs->GetInteger(screen_is_locked_ ? prefs::kPowerLockScreenOffDelayMs :
          prefs::kPowerAcScreenOffDelayMs);
  values.ac_screen_lock_delay_ms =
      prefs->GetInteger(prefs::kPowerAcScreenLockDelayMs);
  values.ac_idle_warning_delay_ms =
      prefs->GetInteger(prefs::kPowerAcIdleWarningDelayMs);
  values.ac_idle_delay_ms =
      prefs->GetInteger(prefs::kPowerAcIdleDelayMs);
  values.battery_screen_dim_delay_ms =
      prefs->GetInteger(screen_is_locked_ ? prefs::kPowerLockScreenDimDelayMs :
          prefs::kPowerBatteryScreenDimDelayMs);
  values.battery_screen_off_delay_ms =
      prefs->GetInteger(screen_is_locked_ ? prefs::kPowerLockScreenOffDelayMs :
          prefs::kPowerBatteryScreenOffDelayMs);
  values.battery_screen_lock_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryScreenLockDelayMs);
  values.battery_idle_warning_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryIdleWarningDelayMs);
  values.battery_idle_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryIdleDelayMs);
  values.ac_idle_action = static_cast<PowerPolicyController::Action>(
      prefs->GetInteger(prefs::kPowerAcIdleAction));
  values.battery_idle_action = static_cast<PowerPolicyController::Action>(
      prefs->GetInteger(prefs::kPowerBatteryIdleAction));
  values.lid_closed_action = static_cast<PowerPolicyController::Action>(
      prefs->GetInteger(prefs::kPowerLidClosedAction));
  values.use_audio_activity =
      prefs->GetBoolean(prefs::kPowerUseAudioActivity);
  values.use_video_activity =
      prefs->GetBoolean(prefs::kPowerUseVideoActivity);
  values.allow_screen_wake_locks =
      prefs->GetBoolean(prefs::kPowerAllowScreenWakeLocks);
  values.enable_auto_screen_lock =
      prefs->GetBoolean(prefs::kEnableAutoScreenLock);
  values.presentation_screen_dim_delay_factor =
      prefs->GetDouble(prefs::kPowerPresentationScreenDimDelayFactor);
  values.user_activity_screen_dim_delay_factor =
      prefs->GetDouble(prefs::kPowerUserActivityScreenDimDelayFactor);
  values.wait_for_initial_user_activity =
      prefs->GetBoolean(prefs::kPowerWaitForInitialUserActivity);
  values.force_nonzero_brightness_for_user_activity =
      prefs->GetBoolean(prefs::kPowerForceNonzeroBrightnessForUserActivity);

  power_policy_controller_->ApplyPrefs(values);
}

// static
void PowerPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPowerAcScreenDimDelayMs, 420000);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenOffDelayMs, 480000);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenLockDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleWarningDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleDelayMs, 1800000);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenDimDelayMs, 300000);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenOffDelayMs, 360000);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenLockDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleWarningDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleDelayMs, 600000);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenDimDelayMs, 30000);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenOffDelayMs, 40000);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleAction,
                                PowerPolicyController::ACTION_SUSPEND);
  registry->RegisterBooleanPref(prefs::kPowerUseAudioActivity, true);
  registry->RegisterBooleanPref(prefs::kPowerUseVideoActivity, true);
  registry->RegisterBooleanPref(prefs::kPowerAllowScreenWakeLocks, true);
  registry->RegisterBooleanPref(
      prefs::kEnableAutoScreenLock,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kPowerPresentationScreenDimDelayFactor,
                               2.0);
  registry->RegisterDoublePref(prefs::kPowerUserActivityScreenDimDelayFactor,
                               2.0);
  registry->RegisterBooleanPref(prefs::kPowerWaitForInitialUserActivity, false);
  registry->RegisterBooleanPref(
      prefs::kPowerForceNonzeroBrightnessForUserActivity, true);
}

void PowerPrefs::SetProfile(Profile* profile) {
  // No need to reapply policy if profile hasn't changed, e.g. when adding a
  // secondary user to an existing session.
  if (profile == profile_)
    return;

  profile_ = profile;
  pref_change_registrar_.reset();

  if (!profile)
    return;

  base::Closure update_callback(base::Bind(
      &PowerPrefs::UpdatePowerPolicyFromPrefs,
      base::Unretained(this)));
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(prefs::kPowerAcScreenDimDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenOffDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenLockDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleWarningDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenDimDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenOffDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenLockDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleWarningDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerLockScreenDimDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerLockScreenOffDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerLidClosedAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerUseAudioActivity, update_callback);
  pref_change_registrar_->Add(prefs::kPowerUseVideoActivity, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAllowScreenWakeLocks,
                              update_callback);
  pref_change_registrar_->Add(prefs::kEnableAutoScreenLock, update_callback);
  pref_change_registrar_->Add(prefs::kPowerPresentationScreenDimDelayFactor,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerUserActivityScreenDimDelayFactor,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerWaitForInitialUserActivity,
                              update_callback);
  pref_change_registrar_->Add(
      prefs::kPowerForceNonzeroBrightnessForUserActivity, update_callback);

  UpdatePowerPolicyFromPrefs();
}

}  // namespace chromeos
