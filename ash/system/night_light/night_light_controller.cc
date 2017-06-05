// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_controller.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

constexpr float kDefaultColorTemperature = 0.5f;

// The duration of the temperature change animation when the change is a result
// of a manual user setting.
// TODO(afakhry): Add automatic schedule animation duration when you implement
// that part. It should be large enough (20 seconds as agreed) to give the user
// a nice smooth transition.
constexpr int kManualToggleAnimationDurationSec = 2;

// Applies the given |layer_temperature| to all the layers of the root windows
// with the given |animation_duration|.
// |layer_temperature| is the ui::Layer floating-point value in the range of
// 0.0f (least warm) to 1.0f (most warm).
void ApplyColorTemperatureToLayers(float layer_temperature,
                                   base::TimeDelta animation_duration) {
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    ui::Layer* layer = root_window->layer();

    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(animation_duration);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    layer->SetLayerTemperature(layer_temperature);
  }
}

}  // namespace

NightLightController::NightLightController(
    SessionController* session_controller)
    : session_controller_(session_controller) {
  session_controller_->AddObserver(this);
}

NightLightController::~NightLightController() {
  session_controller_->RemoveObserver(this);
}

// static
bool NightLightController::IsFeatureEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableNightLight);
}

// static
void NightLightController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNightLightEnabled, false);
  registry->RegisterDoublePref(prefs::kNightLightTemperature,
                               kDefaultColorTemperature);
}

void NightLightController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NightLightController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NightLightController::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kNightLightEnabled);
}

float NightLightController::GetColorTemperature() const {
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kNightLightTemperature);

  return kDefaultColorTemperature;
}

void NightLightController::SetEnabled(bool enabled) {
  if (active_user_pref_service_)
    active_user_pref_service_->SetBoolean(prefs::kNightLightEnabled, enabled);
}

void NightLightController::SetColorTemperature(float temperature) {
  DCHECK_GE(temperature, 0.0f);
  DCHECK_LE(temperature, 1.0f);
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(prefs::kNightLightTemperature,
                                         temperature);
  }
}

void NightLightController::Toggle() {
  SetEnabled(!GetEnabled());
}

void NightLightController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Initial login and user switching in multi profiles.
  pref_change_registrar_.reset();
  active_user_pref_service_ = Shell::Get()->GetActiveUserPrefService();
  InitFromUserPrefs();
}

void NightLightController::Refresh() {
  // TODO(afakhry): Add here refreshing of start and end times, when you
  // implement the automatic schedule settings.
  ApplyColorTemperatureToLayers(
      GetEnabled() ? GetColorTemperature() : 0.0f,
      base::TimeDelta::FromSeconds(kManualToggleAnimationDurationSec));
  Shell::Get()->SetCursorCompositingEnabled(GetEnabled());
}

void NightLightController::StartWatchingPrefsChanges() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = base::MakeUnique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kNightLightEnabled,
      base::Bind(&NightLightController::OnEnabledPrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightTemperature,
      base::Bind(&NightLightController::OnColorTemperaturePrefChanged,
                 base::Unretained(this)));
}

void NightLightController::InitFromUserPrefs() {
  if (!active_user_pref_service_) {
    // The pref_service can be null in ash_unittests.
    return;
  }

  StartWatchingPrefsChanges();
  Refresh();
  NotifyStatusChanged();
}

void NightLightController::NotifyStatusChanged() {
  for (auto& observer : observers_)
    observer.OnNightLightEnabledChanged(GetEnabled());
}

void NightLightController::OnEnabledPrefChanged() {
  DCHECK(active_user_pref_service_);
  Refresh();
  NotifyStatusChanged();
}

void NightLightController::OnColorTemperaturePrefChanged() {
  DCHECK(active_user_pref_service_);
  Refresh();
}

}  // namespace ash
