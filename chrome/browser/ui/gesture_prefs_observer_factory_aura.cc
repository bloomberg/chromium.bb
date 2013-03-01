// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gesture_prefs_observer_factory_aura.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/base/gestures/gesture_configuration.h"

#if defined(USE_ASH)
#include "ash/wm/workspace/workspace_cycler_configuration.h"
#endif  // USE_ASH

#if defined(USE_ASH)
using ash::WorkspaceCyclerConfiguration;
#endif  // USE_ASH

using ui::GestureConfiguration;

namespace {

// TODO(rjkroege): Remove this deprecated pref in M29. http://crbug.com/160243.
const char kTouchScreenFlingAccelerationAdjustment[] =
    "gesture.touchscreen_fling_acceleration_adjustment";

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
        OVERSCROLL_CONFIG_MIN_THRESHOLD_START },
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

#if defined(USE_ASH)
struct WorkspaceCyclerPref {
  const char* pref_name;
  WorkspaceCyclerConfiguration::Property property;
};

const std::vector<WorkspaceCyclerPref>& GetWorkspaceCyclerPrefs() {
  CR_DEFINE_STATIC_LOCAL(std::vector<WorkspaceCyclerPref>, cycler_prefs, ());
  if (cycler_prefs.empty()) {
    const WorkspaceCyclerPref kCyclerPrefs[] = {
      { prefs::kWorkspaceCyclerShallowerThanSelectedYOffsets,
        WorkspaceCyclerConfiguration::SHALLOWER_THAN_SELECTED_Y_OFFSETS },
      { prefs::kWorkspaceCyclerDeeperThanSelectedYOffsets,
        WorkspaceCyclerConfiguration::DEEPER_THAN_SELECTED_Y_OFFSETS },
      { prefs::kWorkspaceCyclerSelectedYOffset,
        WorkspaceCyclerConfiguration::SELECTED_Y_OFFSET },
      { prefs::kWorkspaceCyclerSelectedScale,
        WorkspaceCyclerConfiguration::SELECTED_SCALE },
      { prefs::kWorkspaceCyclerMinScale,
        WorkspaceCyclerConfiguration::MIN_SCALE },
      { prefs::kWorkspaceCyclerMaxScale,
        WorkspaceCyclerConfiguration::MAX_SCALE },
      { prefs::kWorkspaceCyclerMinBrightness,
        WorkspaceCyclerConfiguration::MIN_BRIGHTNESS },
      { prefs::kWorkspaceCyclerBackgroundOpacity,
        WorkspaceCyclerConfiguration::BACKGROUND_OPACITY },
      { prefs::kWorkspaceCyclerDistanceToInitiateCycling,
        WorkspaceCyclerConfiguration::DISTANCE_TO_INITIATE_CYCLING },
      { prefs::kWorkspaceCyclerScrollDistanceToCycleToNextWorkspace,
        WorkspaceCyclerConfiguration::
            SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE },
      { prefs::kWorkspaceCyclerCyclerStepAnimationDurationRatio,
        WorkspaceCyclerConfiguration::CYCLER_STEP_ANIMATION_DURATION_RATIO },
      { prefs::kWorkspaceCyclerStartCyclerAnimationDuration,
        WorkspaceCyclerConfiguration::START_CYCLER_ANIMATION_DURATION },
      { prefs::kWorkspaceCyclerStopCyclerAnimationDuration,
        WorkspaceCyclerConfiguration::STOP_CYCLER_ANIMATION_DURATION },
    };
    cycler_prefs.assign(kCyclerPrefs, kCyclerPrefs + arraysize(kCyclerPrefs));
  }
  return cycler_prefs;
}
#endif  // USE_ASH

// This class manages gesture configuration preferences.
class GesturePrefsObserver : public ProfileKeyedService {
 public:
  explicit GesturePrefsObserver(PrefService* prefs);
  virtual ~GesturePrefsObserver();

  // ProfileKeyedService implementation.
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
  void UpdateWorkspaceCyclerPrefs();

  PrefChangeRegistrar registrar_;
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(GesturePrefsObserver);
};

// The list of prefs we want to observe.
// Note that this collection of settings should correspond to the settings used
// in ui/base/gestures/gesture_configuration.h
const char* kPrefsToObserve[] = {
  prefs::kFlingAccelerationCurveCoefficient0,
  prefs::kFlingAccelerationCurveCoefficient1,
  prefs::kFlingAccelerationCurveCoefficient2,
  prefs::kFlingAccelerationCurveCoefficient3,
  prefs::kFlingMaxCancelToDownTimeInMs,
  prefs::kFlingMaxTapGapTimeInMs,
  prefs::kTabScrubActivationDelayInMS,
  prefs::kFlingVelocityCap,
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

GesturePrefsObserver::GesturePrefsObserver(PrefService* prefs)
    : prefs_(prefs) {
  // Clear for migration.
  prefs->ClearPref(kTouchScreenFlingAccelerationAdjustment);

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

#if defined(USE_ASH)
  const std::vector<WorkspaceCyclerPref>& cycler_prefs =
      GetWorkspaceCyclerPrefs();
  for (size_t i = 0; i < cycler_prefs.size(); ++i)
    registrar_.Add(cycler_prefs[i].pref_name, callback);
#endif  // USE_ASH
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
  GestureConfiguration::set_fling_velocity_cap(
      prefs_->GetDouble(prefs::kFlingVelocityCap));
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

  UpdateOverscrollPrefs();
  UpdateWorkspaceCyclerPrefs();
}

void GesturePrefsObserver::UpdateOverscrollPrefs() {
  const std::vector<OverscrollPref>& overscroll_prefs = GetOverscrollPrefs();
  for (size_t i = 0; i < overscroll_prefs.size(); ++i) {
    content::SetOverscrollConfig(overscroll_prefs[i].config,
        static_cast<float>(prefs_->GetDouble(overscroll_prefs[i].pref_name)));
  }
}

void GesturePrefsObserver::UpdateWorkspaceCyclerPrefs() {
#if defined(USE_ASH)
  const std::vector<WorkspaceCyclerPref>& cycler_prefs =
      GetWorkspaceCyclerPrefs();
  for (size_t i = 0; i < cycler_prefs.size(); ++i) {
    WorkspaceCyclerConfiguration::Property property =
        cycler_prefs[i].property;
    if (WorkspaceCyclerConfiguration::IsListProperty(property)) {
      WorkspaceCyclerConfiguration::SetListValue(property,
          *prefs_->GetList(cycler_prefs[i].pref_name));
    } else {
      WorkspaceCyclerConfiguration::SetDouble(property,
          prefs_->GetDouble(cycler_prefs[i].pref_name));
    }
  }
#endif  // USE_ASH
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
    : ProfileKeyedServiceFactory("GesturePrefsObserverAura",
                                 ProfileDependencyManager::GetInstance()) {}

GesturePrefsObserverFactoryAura::~GesturePrefsObserverFactoryAura() {}

ProfileKeyedService* GesturePrefsObserverFactoryAura::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GesturePrefsObserver(profile->GetPrefs());
}

void GesturePrefsObserverFactoryAura::RegisterOverscrollPrefs(
    PrefRegistrySyncable* registry) {
  const std::vector<OverscrollPref>& overscroll_prefs = GetOverscrollPrefs();

  for (size_t i = 0; i < overscroll_prefs.size(); ++i) {
    registry->RegisterDoublePref(
        overscroll_prefs[i].pref_name,
        content::GetOverscrollConfig(overscroll_prefs[i].config),
        PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}

void GesturePrefsObserverFactoryAura::RegisterFlingCurveParameters(
    PrefRegistrySyncable* registry) {
  content::RendererPreferences def_prefs;

  for (size_t i = 0; i < arraysize(kFlingTouchpadPrefs); i++)
    registry->RegisterDoublePref(kFlingTouchpadPrefs[i],
                                 def_prefs.touchpad_fling_profile[i],
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);

  for (size_t i = 0; i < arraysize(kFlingTouchscreenPrefs); i++)
    registry->RegisterDoublePref(kFlingTouchscreenPrefs[i],
                                 def_prefs.touchscreen_fling_profile[i],
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void GesturePrefsObserverFactoryAura::RegisterWorkspaceCyclerPrefs(
    PrefRegistrySyncable* registry) {
#if defined(USE_ASH)
  const std::vector<WorkspaceCyclerPref>& cycler_prefs =
      GetWorkspaceCyclerPrefs();
  for (size_t i = 0; i < cycler_prefs.size(); ++i) {
    WorkspaceCyclerConfiguration::Property property =
        cycler_prefs[i].property;
    if (WorkspaceCyclerConfiguration::IsListProperty(property)) {
      registry->RegisterListPref(
          cycler_prefs[i].pref_name,
          WorkspaceCyclerConfiguration::GetListValue(property).DeepCopy(),
          PrefRegistrySyncable::UNSYNCABLE_PREF);
    } else {
      registry->RegisterDoublePref(
          cycler_prefs[i].pref_name,
          WorkspaceCyclerConfiguration::GetDouble(property),
          PrefRegistrySyncable::UNSYNCABLE_PREF);
    }
  }
#endif  // USE_ASH
}

void GesturePrefsObserverFactoryAura::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient0,
      GestureConfiguration::fling_acceleration_curve_coefficients(0),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient1,
      GestureConfiguration::fling_acceleration_curve_coefficients(1),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient2,
      GestureConfiguration::fling_acceleration_curve_coefficients(2),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingAccelerationCurveCoefficient3,
      GestureConfiguration::fling_acceleration_curve_coefficients(3),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFlingMaxCancelToDownTimeInMs,
      GestureConfiguration::fling_max_cancel_to_down_time_in_ms(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFlingMaxTapGapTimeInMs,
      GestureConfiguration::fling_max_tap_gap_time_in_ms(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kTabScrubActivationDelayInMS,
      GestureConfiguration::tab_scrub_activation_delay_in_ms(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kFlingVelocityCap,
      GestureConfiguration::fling_velocity_cap(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kLongPressTimeInSeconds,
      GestureConfiguration::long_press_time_in_seconds(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kSemiLongPressTimeInSeconds,
      GestureConfiguration::semi_long_press_time_in_seconds(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxDistanceForTwoFingerTapInPixels,
      GestureConfiguration::max_distance_for_two_finger_tap_in_pixels(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxSecondsBetweenDoubleClick,
      GestureConfiguration::max_seconds_between_double_click(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxSeparationForGestureTouchesInPixels,
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxSwipeDeviationRatio,
      GestureConfiguration::max_swipe_deviation_ratio(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxTouchDownDurationInSecondsForClick,
      GestureConfiguration::max_touch_down_duration_in_seconds_for_click(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxTouchMoveInPixelsForClick,
      GestureConfiguration::max_touch_move_in_pixels_for_click(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMaxDistanceBetweenTapsForDoubleTap,
      GestureConfiguration::max_distance_between_taps_for_double_tap(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinDistanceForPinchScrollInPixels,
      GestureConfiguration::min_distance_for_pinch_scroll_in_pixels(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinFlickSpeedSquared,
      GestureConfiguration::min_flick_speed_squared(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinPinchUpdateDistanceInPixels,
      GestureConfiguration::min_pinch_update_distance_in_pixels(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinRailBreakVelocity,
      GestureConfiguration::min_rail_break_velocity(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinScrollDeltaSquared,
      GestureConfiguration::min_scroll_delta_squared(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinSwipeSpeed,
      GestureConfiguration::min_swipe_speed(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kMinTouchDownDurationInSecondsForClick,
      GestureConfiguration::min_touch_down_duration_in_seconds_for_click(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPointsBufferedForVelocity,
      GestureConfiguration::points_buffered_for_velocity(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kRailBreakProportion,
      GestureConfiguration::rail_break_proportion(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kRailStartProportion,
      GestureConfiguration::rail_start_proportion(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Register for migration.
  registry->RegisterDoublePref(kTouchScreenFlingAccelerationAdjustment,
                               0.0,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);

  RegisterOverscrollPrefs(registry);
  RegisterFlingCurveParameters(registry);
  RegisterWorkspaceCyclerPrefs(registry);
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
