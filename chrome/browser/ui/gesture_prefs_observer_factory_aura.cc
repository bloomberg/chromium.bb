// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/events/gestures/gesture_configuration.h"

using ui::GestureConfiguration;

namespace {

// TODO(tdresser): Remove these deprecated prefs in M38. See crbug.com/379912.

const char kFlingVelocityCap[] = "gesture.fling_velocity_cap";
const char kLongPressTimeInSeconds[] =
    "gesture.long_press_time_in_seconds";
const char kMaxDistanceBetweenTapsForDoubleTap[] =
    "gesture.max_distance_between_taps_for_double_tap";
const char kMaxDistanceForTwoFingerTapInPixels[] =
    "gesture.max_distance_for_two_finger_tap_in_pixels";
const char kMaxSecondsBetweenDoubleClick[] =
    "gesture.max_seconds_between_double_click";
const char kMaxSwipeDeviationRatio[] =
    "gesture.max_swipe_deviation_ratio";
const char kMaxTouchDownDurationInSecondsForClick[] =
    "gesture.max_touch_down_duration_in_seconds_for_click";
const char kMaxTouchMoveInPixelsForClick[] =
    "gesture.max_touch_move_in_pixels_for_click";
const char kMinDistanceForPinchScrollInPixels[] =
    "gesture.min_distance_for_pinch_scroll_in_pixels";
const char kMinFlickSpeedSquared[] =
    "gesture.min_flick_speed_squared";
const char kMinPinchUpdateDistanceInPixels[] =
    "gesture.min_pinch_update_distance_in_pixels";
const char kMinRailBreakVelocity[] =
    "gesture.min_rail_break_velocity";
const char kMinScrollDeltaSquared[] =
    "gesture.min_scroll_delta_squared";
const char kMinSwipeSpeed[] =
    "gesture.min_swipe_speed";
const char kMinTouchDownDurationInSecondsForClick[] =
    "gesture.min_touch_down_duration_in_seconds_for_click";
const char kPointsBufferedForVelocity[] =
    "gesture.points_buffered_for_velocity";
const char kRailBreakProportion[] =
    "gesture.rail_break_proportion";
const char kRailStartProportion[] =
    "gesture.rail_start_proportion";
const char kScrollPredictionSeconds[] =
    "gesture.scroll_prediction_seconds";
const char kShowPressDelayInMS[] =
    "gesture.show_press_delay_in_ms";

struct OverscrollPref {
  const char* pref_name;
  content::OverscrollConfig config;
};

const std::vector<OverscrollPref>& GetOverscrollPrefs() {
  CR_DEFINE_STATIC_LOCAL(std::vector<OverscrollPref>, overscroll_prefs, ());
  if (overscroll_prefs.empty()) {
    using namespace content;
    const OverscrollPref kOverscrollPrefs[] = {
      { prefs::kOverscrollHorizontalThresholdComplete,
        OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE },
      { prefs::kOverscrollVerticalThresholdComplete,
        OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE },
      { prefs::kOverscrollMinimumThresholdStart,
        OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN },
      { prefs::kOverscrollMinimumThresholdStartTouchpad,
        OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD },
      { prefs::kOverscrollVerticalThresholdStart,
        OVERSCROLL_CONFIG_VERT_THRESHOLD_START },
      { prefs::kOverscrollHorizontalResistThreshold,
        OVERSCROLL_CONFIG_HORIZ_RESIST_AFTER },
      { prefs::kOverscrollVerticalResistThreshold,
        OVERSCROLL_CONFIG_VERT_RESIST_AFTER },
    };
    overscroll_prefs.assign(kOverscrollPrefs,
                            kOverscrollPrefs + arraysize(kOverscrollPrefs));
  }
  return overscroll_prefs;
}

// This class manages gesture configuration preferences.
class GesturePrefsObserver : public KeyedService {
 public:
  explicit GesturePrefsObserver(PrefService* prefs);
  virtual ~GesturePrefsObserver();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

 private:
  // Notification callback invoked when browser-side preferences
  // are updated and need to be pushed into ui::GesturePreferences.
  void Update();

  // Notification callback invoked when the fling deacceleration
  // gesture preferences are changed from chrome://gesture.
  // Broadcasts the changes all renderers where they are used.
  void Notify();

  // Notification helper to push overscroll preferences into
  // content.
  void UpdateOverscrollPrefs();

  PrefChangeRegistrar registrar_;
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserver);
};

// The list of prefs we want to observe.
// Note that this collection of settings should correspond to the settings used
// in ui/events/gestures/gesture_configuration.h
const char* kPrefsToObserve[] = {
  prefs::kFlingAccelerationCurveCoefficient0,
  prefs::kFlingAccelerationCurveCoefficient1,
  prefs::kFlingAccelerationCurveCoefficient2,
  prefs::kFlingAccelerationCurveCoefficient3,
  prefs::kFlingMaxCancelToDownTimeInMs,
  prefs::kFlingMaxTapGapTimeInMs,
  prefs::kTabScrubActivationDelayInMS,
  prefs::kMaxSeparationForGestureTouchesInPixels,
  prefs::kSemiLongPressTimeInSeconds,
};

const char* kFlingTouchpadPrefs[] = {
  prefs::kFlingCurveTouchpadAlpha,
  prefs::kFlingCurveTouchpadBeta,
  prefs::kFlingCurveTouchpadGamma
};

const char* kFlingTouchscreenPrefs[] = {
  prefs::kFlingCurveTouchscreenAlpha,
  prefs::kFlingCurveTouchscreenBeta,
  prefs::kFlingCurveTouchscreenGamma,
};

const char* kPrefsToRemove[] = {
  kFlingVelocityCap,
  kLongPressTimeInSeconds,
  kMaxDistanceBetweenTapsForDoubleTap,
  kMaxDistanceForTwoFingerTapInPixels,
  kMaxSecondsBetweenDoubleClick,
  kMaxSwipeDeviationRatio,
  kMaxTouchDownDurationInSecondsForClick,
  kMaxTouchMoveInPixelsForClick,
  kMinDistanceForPinchScrollInPixels,
  kMinFlickSpeedSquared,
  kMinPinchUpdateDistanceInPixels,
  kMinRailBreakVelocity,
  kMinScrollDeltaSquared,
  kMinSwipeSpeed,
  kMinTouchDownDurationInSecondsForClick,
  kPointsBufferedForVelocity,
  kRailBreakProportion,
  kRailStartProportion,
  kScrollPredictionSeconds,
  kShowPressDelayInMS,
};

GesturePrefsObserver::GesturePrefsObserver(PrefService* prefs)
    : prefs_(prefs) {
  // Clear for migration.
  for (size_t i = 0; i < arraysize(kPrefsToRemove); ++i) {
    if (prefs->FindPreference(kPrefsToRemove[i]))
      prefs->ClearPref(kPrefsToRemove[i]);
  }

  registrar_.Init(prefs);
  registrar_.RemoveAll();
  base::Closure callback = base::Bind(&GesturePrefsObserver::Update,
                                      base::Unretained(this));

  base::Closure notify_callback = base::Bind(&GesturePrefsObserver::Notify,
                                             base::Unretained(this));

  for (size_t i = 0; i < arraysize(kPrefsToObserve); ++i)
    registrar_.Add(kPrefsToObserve[i], callback);

  const std::vector<OverscrollPref>& overscroll_prefs = GetOverscrollPrefs();
  for (size_t i = 0; i < overscroll_prefs.size(); ++i)
    registrar_.Add(overscroll_prefs[i].pref_name, callback);

  for (size_t i = 0; i < arraysize(kFlingTouchpadPrefs); ++i)
    registrar_.Add(kFlingTouchpadPrefs[i], notify_callback);
  for (size_t i = 0; i < arraysize(kFlingTouchscreenPrefs); ++i)
    registrar_.Add(kFlingTouchscreenPrefs[i], notify_callback);

  Update();
}

GesturePrefsObserver::~GesturePrefsObserver() {}

void GesturePrefsObserver::Shutdown() {
  registrar_.RemoveAll();
}

void GesturePrefsObserver::Update() {
  GestureConfiguration::set_fling_acceleration_curve_coefficients(0,
      prefs_->GetDouble(prefs::kFlingAccelerationCurveCoefficient0));
  GestureConfiguration::set_fling_acceleration_curve_coefficients(1,
      prefs_->GetDouble(prefs::kFlingAccelerationCurveCoefficient1));
  GestureConfiguration::set_fling_acceleration_curve_coefficients(2,
      prefs_->GetDouble(prefs::kFlingAccelerationCurveCoefficient2));
  GestureConfiguration::set_fling_acceleration_curve_coefficients(3,
      prefs_->GetDouble(prefs::kFlingAccelerationCurveCoefficient3));
  GestureConfiguration::set_fling_max_cancel_to_down_time_in_ms(
      prefs_->GetInteger(prefs::kFlingMaxCancelToDownTimeInMs));
  GestureConfiguration::set_fling_max_tap_gap_time_in_ms(
      prefs_->GetInteger(prefs::kFlingMaxTapGapTimeInMs));
  GestureConfiguration::set_tab_scrub_activation_delay_in_ms(
      prefs_->GetInteger(prefs::kTabScrubActivationDelayInMS));
  GestureConfiguration::set_semi_long_press_time_in_seconds(
      prefs_->GetDouble(
          prefs::kSemiLongPressTimeInSeconds));
  GestureConfiguration::set_max_separation_for_gesture_touches_in_pixels(
      prefs_->GetDouble(
          prefs::kMaxSeparationForGestureTouchesInPixels));

  UpdateOverscrollPrefs();
}

void GesturePrefsObserver::UpdateOverscrollPrefs() {
  const std::vector<OverscrollPref>& overscroll_prefs = GetOverscrollPrefs();
  for (size_t i = 0; i < overscroll_prefs.size(); ++i) {
    content::SetOverscrollConfig(overscroll_prefs[i].config,
        static_cast<float>(prefs_->GetDouble(overscroll_prefs[i].pref_name)));
  }
}

void GesturePrefsObserver::Notify() {
  // Must do a notify to distribute the changes to all renderers.
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_BROWSER_FLING_CURVE_PARAMETERS_CHANGED,
                  content::Source<GesturePrefsObserver>(this),
                  content::NotificationService::NoDetails());
}

}  // namespace

// static
GesturePrefsObserverFactoryAura*
GesturePrefsObserverFactoryAura::GetInstance() {
  return Singleton<GesturePrefsObserverFactoryAura>::get();
}

GesturePrefsObserverFactoryAura::GesturePrefsObserverFactoryAura()
    : BrowserContextKeyedServiceFactory(
        "GesturePrefsObserverAura",
        BrowserContextDependencyManager::GetInstance()) {}

GesturePrefsObserverFactoryAura::~GesturePrefsObserverFactoryAura() {}

KeyedService* GesturePrefsObserverFactoryAura::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new GesturePrefsObserver(static_cast<Profile*>(profile)->GetPrefs());
}

void GesturePrefsObserverFactoryAura::RegisterOverscrollPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  const std::vector<OverscrollPref>& overscroll_prefs = GetOverscrollPrefs();

  for (size_t i = 0; i < overscroll_prefs.size(); ++i) {
    registry->RegisterDoublePref(
        overscroll_prefs[i].pref_name,
        content::GetOverscrollConfig(overscroll_prefs[i].config),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}

void GesturePrefsObserverFactoryAura::RegisterFlingCurveParameters(
    user_prefs::PrefRegistrySyncable* registry) {
  content::RendererPreferences def_prefs;

  for (size_t i = 0; i < arraysize(kFlingTouchpadPrefs); i++)
    registry->RegisterDoublePref(
        kFlingTouchpadPrefs[i],
        def_prefs.touchpad_fling_profile[i],
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  for (size_t i = 0; i < arraysize(kFlingTouchscreenPrefs); i++)
    registry->RegisterDoublePref(
        kFlingTouchscreenPrefs[i],
        def_prefs.touchscreen_fling_profile[i],
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void GesturePrefsObserverFactoryAura::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient0,
      GestureConfiguration::fling_acceleration_curve_coefficients(0),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient1,
      GestureConfiguration::fling_acceleration_curve_coefficients(1),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient2,
      GestureConfiguration::fling_acceleration_curve_coefficients(2),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient3,
      GestureConfiguration::fling_acceleration_curve_coefficients(3),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFlingMaxCancelToDownTimeInMs,
      GestureConfiguration::fling_max_cancel_to_down_time_in_ms(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFlingMaxTapGapTimeInMs,
      GestureConfiguration::fling_max_tap_gap_time_in_ms(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kTabScrubActivationDelayInMS,
      GestureConfiguration::tab_scrub_activation_delay_in_ms(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kSemiLongPressTimeInSeconds,
      GestureConfiguration::semi_long_press_time_in_seconds(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxSeparationForGestureTouchesInPixels,
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  RegisterOverscrollPrefs(registry);
  RegisterFlingCurveParameters(registry);
}

bool
GesturePrefsObserverFactoryAura::ServiceIsCreatedWithBrowserContext() const {
  // Create the observer as soon as the profile is created.
  return true;
}

content::BrowserContext*
GesturePrefsObserverFactoryAura::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use same gesture preferences on incognito windows.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool GesturePrefsObserverFactoryAura::ServiceIsNULLWhileTesting() const {
  // Some tests replace the PrefService of the TestingProfile after the
  // GesturePrefsObserver has been created, which makes Shutdown()
  // remove the registrar from a non-existent PrefService.
  return true;
}
