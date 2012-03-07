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

using aura::GestureConfiguration;

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
  PrefService* local_state_;
  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserverAura);
};

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

GesturePrefsObserverAura::GesturePrefsObserverAura()
    : local_state_(0) {
}

GesturePrefsObserverAura::~GesturePrefsObserverAura() {
}

GesturePrefsObserverAura* GesturePrefsObserverAura::GetInstance() {
  return Singleton<GesturePrefsObserverAura,
    LeakySingletonTraits<GesturePrefsObserverAura> >::get();
}

void GesturePrefsObserverAura::RegisterPrefs(PrefService* local_state) {
  if (local_state_ == 0) {
    local_state_ = local_state;

    if (local_state_) {
      local_state_->RegisterDoublePref(
        prefs::kMaximumSecondsBetweenDoubleClick, 0.7);
      local_state_->RegisterDoublePref(
        prefs::kMaximumTouchDownDurationInSecondsForClick, 0.8);
      local_state_->RegisterDoublePref(
        prefs::kMaximumTouchMoveInPixelsForClick, 20);
      local_state_->RegisterDoublePref(
        prefs::kMinFlickSpeedSquared, 550.f*550.f);
      local_state_->RegisterDoublePref(
        prefs::kMinimumTouchDownDurationInSecondsForClick, 0.01);

      registrar_.Init(local_state_);
      registrar_.RemoveAll();
      for (int i = 0; i < kPrefsToObserveLength; ++i)
        registrar_.Add(kPrefsToObserve[i], this);
    }
  }
}

void GesturePrefsObserverAura::Update() {
  if (local_state_) {
    GestureConfiguration::set_max_seconds_between_double_click(
      local_state_->GetDouble(prefs::kMaximumSecondsBetweenDoubleClick));
    GestureConfiguration::set_max_touch_down_duration_in_seconds_for_click(
      local_state_->GetDouble(
      prefs::kMaximumTouchDownDurationInSecondsForClick));
    GestureConfiguration::set_max_touch_move_in_pixels_for_click(
      local_state_->GetDouble(prefs::kMaximumTouchMoveInPixelsForClick));
    GestureConfiguration::set_min_flick_speed_squared(
      local_state_->GetDouble(prefs::kMinFlickSpeedSquared));
    GestureConfiguration::set_min_touch_down_duration_in_seconds_for_click(
      local_state_->GetDouble(
      prefs::kMinimumTouchDownDurationInSecondsForClick));
  }
}

void GesturePrefsObserverAura::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  Update();
}

} // namespace

void GesturePrefsRegisterPrefs(PrefService* prefs) {
  GesturePrefsObserverAura::GetInstance()->RegisterPrefs(prefs);
}
