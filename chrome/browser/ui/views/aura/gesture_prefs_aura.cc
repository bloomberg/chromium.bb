// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/gesture_prefs.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_observer.h"
#include "ui/aura/gestures/gesture_configuration.h"

namespace {

// This class manages gesture configuration preferences.
class GesturePrefsObserverAura : public content::NotificationObserver {
 public:
  static GesturePrefsObserverAura *GetInstance();
  void RegisterPrefs(PrefService* prefs);

 private:
  GesturePrefsObserverAura();
  friend struct DefaultSingletonTraits<GesturePrefsObserverAura>;
  virtual ~GesturePrefsObserverAura();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void Update();

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserverAura);
};

GesturePrefsObserverAura::GesturePrefsObserverAura() {
}

GesturePrefsObserverAura::~GesturePrefsObserverAura() {
}

GesturePrefsObserverAura* GesturePrefsObserverAura::GetInstance() {
  return Singleton<GesturePrefsObserverAura>::get();
}

// The list of prefs we want to observe.
// Note that this collection of settings should correspond to the settings used
// in ui/aura/gestures/gesture_configuration.h
const char* kPrefsToObserve[] = {
  prefs::kMaximumSecondsBetweenDoubleClick,
  prefs::kMaximumTouchDownDurationInSecondsForClick,
  prefs::kMaximumTouchMoveInPixelsForClick,
  prefs::kMinFlickSpeedSquared,
  prefs::kMinimumTouchDownDurationInSecondsForClick,
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

void GesturePrefsObserverAura::RegisterPrefs(PrefService* ) {
  PrefService* local_state = g_browser_process->local_state();

  local_state->RegisterDoublePref(
    prefs::kMaximumSecondsBetweenDoubleClick, 0.7);
  local_state->RegisterDoublePref(
    prefs::kMaximumTouchDownDurationInSecondsForClick, 0.8);
  local_state->RegisterDoublePref(
    prefs::kMaximumTouchMoveInPixelsForClick, 20);
  local_state->RegisterDoublePref(
    prefs::kMinFlickSpeedSquared, 550.f*550.f);
  local_state->RegisterDoublePref(
    prefs::kMinimumTouchDownDurationInSecondsForClick, 0.01);

  registrar_.Init(local_state);
  if (local_state) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      registrar_.Add(kPrefsToObserve[i], this);
  }
}

void GesturePrefsObserverAura::Update() {
  PrefService* local_state = g_browser_process->local_state();

  aura::GestureConfiguration::set_max_seconds_between_double_click(
    local_state->GetDouble(prefs::kMaximumSecondsBetweenDoubleClick));
  aura::GestureConfiguration::set_max_touch_down_duration_in_seconds_for_click(
    local_state->GetDouble(prefs::kMaximumTouchDownDurationInSecondsForClick));
  aura::GestureConfiguration::set_max_touch_move_in_pixels_for_click(
    local_state->GetDouble(prefs::kMaximumTouchMoveInPixelsForClick));
  aura::GestureConfiguration::set_min_flick_speed_squared(
    local_state->GetDouble(prefs::kMinFlickSpeedSquared));
  aura::GestureConfiguration::set_min_touch_down_duration_in_seconds_for_click(
    local_state->GetDouble(prefs::kMinimumTouchDownDurationInSecondsForClick));
}

void GesturePrefsObserverAura::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  Update();
}

}  // namespace

void GesturePrefsRegisterPrefs(PrefService* prefs) {
  GesturePrefsObserverAura::GetInstance()->RegisterPrefs(prefs);
}
