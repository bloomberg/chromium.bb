// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"

#include "base/compiler_specific.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/gestures/gesture_configuration.h"

using ui::GestureConfiguration;

namespace {

// This class manages gesture configuration preferences.
class GesturePrefsObserver : public content::NotificationObserver,
                             public ProfileKeyedService {
 public:
  explicit GesturePrefsObserver(PrefService* prefs);
  virtual ~GesturePrefsObserver();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void Update();

  PrefChangeRegistrar registrar_;
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserver);
};

// The list of prefs we want to observe.
// Note that this collection of settings should correspond to the settings used
// in ui/base/gestures/gesture_configuration.h
const char* kPrefsToObserve[] = {
  prefs::kLongPressTimeInSeconds,
  prefs::kMaxDistanceForTwoFingerTapInPixels,
  prefs::kMaxSecondsBetweenDoubleClick,
  prefs::kMaxSeparationForGestureTouchesInPixels,
  prefs::kMaxSwipeDeviationRatio,
  prefs::kMaxTouchDownDurationInSecondsForClick,
  prefs::kMaxTouchMoveInPixelsForClick,
  prefs::kMinDistanceForPinchScrollInPixels,
  prefs::kMinFlickSpeedSquared,
  prefs::kMinPinchUpdateDistanceInPixels,
  prefs::kMinRailBreakVelocity,
  prefs::kMinScrollDeltaSquared,
  prefs::kMinSwipeSpeed,
  prefs::kMinTouchDownDurationInSecondsForClick,
  prefs::kPointsBufferedForVelocity,
  prefs::kRailBreakProportion,
  prefs::kRailStartProportion,
  prefs::kSemiLongPressTimeInSeconds,
  prefs::kTouchScreenFlingAccelerationAdjustment,
};

GesturePrefsObserver::GesturePrefsObserver(PrefService* prefs)
    : prefs_(prefs) {
  registrar_.Init(prefs);
  registrar_.RemoveAll();
  for (size_t i = 0; i < arraysize(kPrefsToObserve); ++i)
    registrar_.Add(kPrefsToObserve[i], this);
}

GesturePrefsObserver::~GesturePrefsObserver() {}

void GesturePrefsObserver::Shutdown() {
  registrar_.RemoveAll();
}

void GesturePrefsObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Update();
}

void GesturePrefsObserver::Update() {
  GestureConfiguration::set_long_press_time_in_seconds(
      prefs_->GetDouble(
          prefs::kLongPressTimeInSeconds));
  GestureConfiguration::set_semi_long_press_time_in_seconds(
      prefs_->GetDouble(
          prefs::kSemiLongPressTimeInSeconds));
  GestureConfiguration::set_max_distance_for_two_finger_tap_in_pixels(
      prefs_->GetDouble(
          prefs::kMaxDistanceForTwoFingerTapInPixels));
  GestureConfiguration::set_max_seconds_between_double_click(
      prefs_->GetDouble(
          prefs::kMaxSecondsBetweenDoubleClick));
  GestureConfiguration::set_max_separation_for_gesture_touches_in_pixels(
      prefs_->GetDouble(
          prefs::kMaxSeparationForGestureTouchesInPixels));
  GestureConfiguration::set_max_swipe_deviation_ratio(
      prefs_->GetDouble(
          prefs::kMaxSwipeDeviationRatio));
  GestureConfiguration::set_max_touch_down_duration_in_seconds_for_click(
      prefs_->GetDouble(
          prefs::kMaxTouchDownDurationInSecondsForClick));
  GestureConfiguration::set_max_touch_move_in_pixels_for_click(
      prefs_->GetDouble(
          prefs::kMaxTouchMoveInPixelsForClick));
  GestureConfiguration::set_max_distance_between_taps_for_double_tap(
      prefs_->GetDouble(
          prefs::kMaxDistanceBetweenTapsForDoubleTap));
  GestureConfiguration::set_min_distance_for_pinch_scroll_in_pixels(
      prefs_->GetDouble(
          prefs::kMinDistanceForPinchScrollInPixels));
  GestureConfiguration::set_min_flick_speed_squared(
      prefs_->GetDouble(
          prefs::kMinFlickSpeedSquared));
  GestureConfiguration::set_min_pinch_update_distance_in_pixels(
      prefs_->GetDouble(
          prefs::kMinPinchUpdateDistanceInPixels));
  GestureConfiguration::set_min_rail_break_velocity(
      prefs_->GetDouble(
          prefs::kMinRailBreakVelocity));
  GestureConfiguration::set_min_scroll_delta_squared(
      prefs_->GetDouble(
          prefs::kMinScrollDeltaSquared));
  GestureConfiguration::set_min_swipe_speed(
      prefs_->GetDouble(
          prefs::kMinSwipeSpeed));
  GestureConfiguration::set_min_touch_down_duration_in_seconds_for_click(
      prefs_->GetDouble(
          prefs::kMinTouchDownDurationInSecondsForClick));
  GestureConfiguration::set_points_buffered_for_velocity(
      prefs_->GetInteger(
          prefs::kPointsBufferedForVelocity));
  GestureConfiguration::set_rail_break_proportion(
      prefs_->GetDouble(
          prefs::kRailBreakProportion));
  GestureConfiguration::set_rail_start_proportion(
      prefs_->GetDouble(
          prefs::kRailStartProportion));
  GestureConfiguration::set_touchscreen_fling_acceleration_adjustment(
      prefs_->GetDouble(
          prefs::kTouchScreenFlingAccelerationAdjustment));
}

}  // namespace

// static
GesturePrefsObserverFactoryAura*
GesturePrefsObserverFactoryAura::GetInstance() {
  return Singleton<GesturePrefsObserverFactoryAura>::get();
}

GesturePrefsObserverFactoryAura::GesturePrefsObserverFactoryAura()
    : ProfileKeyedServiceFactory("GesturePrefsObserverAura",
                                 ProfileDependencyManager::GetInstance()) {}

GesturePrefsObserverFactoryAura::~GesturePrefsObserverFactoryAura() {}

ProfileKeyedService* GesturePrefsObserverFactoryAura::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GesturePrefsObserver(profile->GetPrefs());
}

void GesturePrefsObserverFactoryAura::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDoublePref(
      prefs::kLongPressTimeInSeconds,
      GestureConfiguration::long_press_time_in_seconds(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kSemiLongPressTimeInSeconds,
      GestureConfiguration::semi_long_press_time_in_seconds(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxDistanceForTwoFingerTapInPixels,
      GestureConfiguration::max_distance_for_two_finger_tap_in_pixels(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxSecondsBetweenDoubleClick,
      GestureConfiguration::max_seconds_between_double_click(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxSeparationForGestureTouchesInPixels,
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxSwipeDeviationRatio,
      GestureConfiguration::max_swipe_deviation_ratio(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxTouchDownDurationInSecondsForClick,
      GestureConfiguration::max_touch_down_duration_in_seconds_for_click(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxTouchMoveInPixelsForClick,
      GestureConfiguration::max_touch_move_in_pixels_for_click(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMaxDistanceBetweenTapsForDoubleTap,
      GestureConfiguration::max_distance_between_taps_for_double_tap(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinDistanceForPinchScrollInPixels,
      GestureConfiguration::min_distance_for_pinch_scroll_in_pixels(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinFlickSpeedSquared,
      GestureConfiguration::min_flick_speed_squared(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinPinchUpdateDistanceInPixels,
      GestureConfiguration::min_pinch_update_distance_in_pixels(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinRailBreakVelocity,
      GestureConfiguration::min_rail_break_velocity(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinScrollDeltaSquared,
      GestureConfiguration::min_scroll_delta_squared(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinSwipeSpeed,
      GestureConfiguration::min_swipe_speed(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kMinTouchDownDurationInSecondsForClick,
      GestureConfiguration::min_touch_down_duration_in_seconds_for_click(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(
      prefs::kPointsBufferedForVelocity,
      GestureConfiguration::points_buffered_for_velocity(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kRailBreakProportion,
      GestureConfiguration::rail_break_proportion(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kRailStartProportion,
      GestureConfiguration::rail_start_proportion(),
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(
      prefs::kTouchScreenFlingAccelerationAdjustment,
      GestureConfiguration::touchscreen_fling_acceleration_adjustment(),
      PrefService::UNSYNCABLE_PREF);
}

bool GesturePrefsObserverFactoryAura::ServiceIsCreatedWithProfile() const {
  // Create the observer as soon as the profile is created.
  return true;
}

bool GesturePrefsObserverFactoryAura::ServiceRedirectedInIncognito() const {
  // Use same gesture preferences on incognito windows.
  return true;
}

bool GesturePrefsObserverFactoryAura::ServiceIsNULLWhileTesting() const {
  // Some tests replace the PrefService of the TestingProfile after the
  // GesturePrefsObserver has been created, which makes Shutdown()
  // remove the registrar from a non-existent PrefService.
  return true;
}
