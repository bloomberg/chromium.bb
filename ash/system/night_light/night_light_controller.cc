// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_controller.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

// The key of the dictionary value in the user's pref service that contains all
// the NightLight settings.
constexpr char kNightLightPrefsKey[] = "prefs.night_light_prefs";

// Keys to the various NightLight settings inside its dictionary value.
constexpr char kStatusKey[] = "night_light_status";
constexpr char kColorTemperatureKey[] = "night_light_color_temperature";

// The duration of the temperature change animation when the change is a result
// of a manual user setting.
// TODO(afakhry): Add automatic schedule animation duration when you implement
// that part. It should be large enough (20 seconds as agreed) to give the user
// a nice smooth transition.
constexpr int kManualToggleAnimationDurationSec = 2;

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
void NightLightController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNightLightPrefsKey);
}

void NightLightController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NightLightController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NightLightController::Toggle() {
  SetEnabled(!enabled_);
}

void NightLightController::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  Refresh();
  NotifyStatusChanged();
  PersistUserPrefs();
}

void NightLightController::SetColorTemperature(float temperature) {
  // TODO(afakhry): Spport changing the temperature when you implement the
  // settings part of this feature. Right now we'll keep it fixed at the value
  // |color_temperature_| whenever NightLight is turned on.

  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    ui::Layer* layer = root_window->layer();

    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromSeconds(kManualToggleAnimationDurationSec));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    layer->SetLayerTemperature(temperature);
  }
}

void NightLightController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Initial login and user switching in multi profiles.
  InitFromUserPrefs();
}

void NightLightController::Refresh() {
  SetColorTemperature(enabled_ ? color_temperature_ : 0.0f);
}

void NightLightController::InitFromUserPrefs() {
  auto* pref_service = Shell::Get()->GetActiveUserPrefService();
  if (!pref_service) {
    // The pref_service can be NULL in ash_unittests.
    return;
  }

  const base::DictionaryValue* night_light_prefs =
      pref_service->GetDictionary(kNightLightPrefsKey);
  bool enabled = false;
  night_light_prefs->GetBoolean(kStatusKey, &enabled);
  enabled_ = enabled;

  double color_temperature = 0.5;
  night_light_prefs->GetDouble(kColorTemperatureKey, &color_temperature);
  color_temperature_ = static_cast<float>(color_temperature);

  Refresh();
  NotifyStatusChanged();
}

void NightLightController::PersistUserPrefs() {
  auto* pref_service = ash::Shell::Get()->GetActiveUserPrefService();
  if (!pref_service) {
    // The pref_service can be NULL in ash_unittests.
    return;
  }
  DictionaryPrefUpdate pref_updater(pref_service, kNightLightPrefsKey);

  base::DictionaryValue* dictionary = pref_updater.Get();
  dictionary->SetBoolean(kStatusKey, enabled_);
  dictionary->SetDouble(kColorTemperatureKey,
                        static_cast<double>(color_temperature_));
}

void NightLightController::NotifyStatusChanged() {
  for (auto& observer : observers_)
    observer.OnNightLightEnabledChanged(enabled_);
}

}  // namespace ash
