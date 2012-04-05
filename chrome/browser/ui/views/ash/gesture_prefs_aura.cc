// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/gesture_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/gestures/gesture_configuration.h"

using ui::GestureConfiguration;

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
// in ui/base/gestures/gesture_configuration.h
const char* kPrefsToObserve[] = {
  prefs::kLongPressTimeInSeconds,
  prefs::kMaxSecondsBetweenDoubleClick,
  prefs::kMaxSeparationForGestureTouchesInPixels,
  prefs::kMaxTouchDownDurationInSecondsForClick,
  prefs::kMaxTouchMoveInPixelsForClick,
  prefs::kMinDistanceForPinchScrollInPixels,
  prefs::kMinFlickSpeedSquared,
  prefs::kMinPinchUpdateDistanceInPixels,
  prefs::kMinRailBreakVelocity,
  prefs::kMinScrollDeltaSquared,
  prefs::kMinTouchDownDurationInSecondsForClick,
  prefs::kPointsBufferedForVelocity,
  prefs::kRailBreakProportion,
  prefs::kRailStartProportion,
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
      local_state->RegisterDoublePref(
          prefs::kLongPressTimeInSeconds,
          GestureConfiguration::long_press_time_in_seconds());
      local_state_->RegisterDoublePref(
          prefs::kMaxSecondsBetweenDoubleClick,
          GestureConfiguration::max_seconds_between_double_click());
      local_state_->RegisterDoublePref(
          prefs::kMaxSeparationForGestureTouchesInPixels,
          GestureConfiguration::max_separation_for_gesture_touches_in_pixels());
      local_state_->RegisterDoublePref(
          prefs::kMaxTouchDownDurationInSecondsForClick,
          GestureConfiguration::max_touch_down_duration_in_seconds_for_click());
      local_state_->RegisterDoublePref(
          prefs::kMaxTouchMoveInPixelsForClick,
          GestureConfiguration::max_touch_move_in_pixels_for_click());
      local_state->RegisterDoublePref(
          prefs::kMinDistanceForPinchScrollInPixels,
          GestureConfiguration::min_distance_for_pinch_scroll_in_pixels());
      local_state_->RegisterDoublePref(
          prefs::kMinFlickSpeedSquared,
          GestureConfiguration::min_flick_speed_squared());
      local_state->RegisterDoublePref(
          prefs::kMinPinchUpdateDistanceInPixels,
          GestureConfiguration::min_pinch_update_distance_in_pixels());
      local_state_->RegisterDoublePref(
          prefs::kMinRailBreakVelocity,
          GestureConfiguration::min_rail_break_velocity());
      local_state_->RegisterDoublePref(
          prefs::kMinScrollDeltaSquared,
          GestureConfiguration::min_scroll_delta_squared());
      local_state_->RegisterDoublePref(
          prefs::kMinTouchDownDurationInSecondsForClick,
          GestureConfiguration::min_touch_down_duration_in_seconds_for_click());
      local_state->RegisterIntegerPref(
          prefs::kPointsBufferedForVelocity,
          GestureConfiguration::points_buffered_for_velocity());
      local_state->RegisterDoublePref(
          prefs::kRailBreakProportion,
          GestureConfiguration::rail_break_proportion());
      local_state->RegisterDoublePref(
          prefs::kRailStartProportion,
          GestureConfiguration::rail_start_proportion());

      registrar_.Init(local_state_);
      registrar_.RemoveAll();
      for (int i = 0; i < kPrefsToObserveLength; ++i)
        registrar_.Add(kPrefsToObserve[i], this);
    }
  }
}

void GesturePrefsObserverAura::Update() {
  if (local_state_) {
    GestureConfiguration::set_long_press_time_in_seconds(
        local_state_->GetDouble(
            prefs::kLongPressTimeInSeconds));
    GestureConfiguration::set_max_seconds_between_double_click(
        local_state_->GetDouble(
            prefs::kMaxSecondsBetweenDoubleClick));
    GestureConfiguration::set_max_separation_for_gesture_touches_in_pixels(
        local_state_->GetDouble(
            prefs::kMaxSeparationForGestureTouchesInPixels));
    GestureConfiguration::set_max_touch_down_duration_in_seconds_for_click(
        local_state_->GetDouble(
            prefs::kMaxTouchDownDurationInSecondsForClick));
    GestureConfiguration::set_max_touch_move_in_pixels_for_click(
        local_state_->GetDouble(
            prefs::kMaxTouchMoveInPixelsForClick));
    GestureConfiguration::set_min_distance_for_pinch_scroll_in_pixels(
        local_state_->GetDouble(
            prefs::kMinDistanceForPinchScrollInPixels));
    GestureConfiguration::set_min_flick_speed_squared(
        local_state_->GetDouble(
            prefs::kMinFlickSpeedSquared));
    GestureConfiguration::set_min_pinch_update_distance_in_pixels(
        local_state_->GetDouble(
            prefs::kMinPinchUpdateDistanceInPixels));
    GestureConfiguration::set_min_rail_break_velocity(
        local_state_->GetDouble(
            prefs::kMinRailBreakVelocity));
    GestureConfiguration::set_min_scroll_delta_squared(
        local_state_->GetDouble(
            prefs::kMinScrollDeltaSquared));
    GestureConfiguration::set_min_touch_down_duration_in_seconds_for_click(
        local_state_->GetDouble(
            prefs::kMinTouchDownDurationInSecondsForClick));
    GestureConfiguration::set_points_buffered_for_velocity(
        local_state_->GetInteger(
            prefs::kPointsBufferedForVelocity));
    GestureConfiguration::set_rail_break_proportion(
        local_state_->GetDouble(
            prefs::kRailBreakProportion));
    GestureConfiguration::set_rail_start_proportion(
        local_state_->GetDouble(
            prefs::kRailStartProportion));
  }
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
