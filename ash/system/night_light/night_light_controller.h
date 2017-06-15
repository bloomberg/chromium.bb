// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/night_light_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/system/night_light/time_of_day.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/binding.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

class SessionController;

// Controls the NightLight feature that adjusts the color temperature of the
// screen.
class ASH_EXPORT NightLightController
    : public NON_EXPORTED_BASE(mojom::NightLightController),
      public SessionObserver {
 public:
  using ScheduleType = mojom::NightLightController::ScheduleType;

  enum class AnimationDuration {
    // Short animation (2 seconds) used for manual changes of NightLight status
    // and temperature by the user.
    kShort,

    // Long animation (20 seconds) used for applying the color temperature
    // gradually as a result of getting into or out of the automatically
    // scheduled NightLight mode. This gives the user a smooth transition.
    kLong,
  };

  // This class enables us to inject fake values for "Now" as well as the sunset
  // and sunrise times, so that we can reliably test the behavior in various
  // schedule types and times.
  class Delegate {
   public:
    // NightLightController owns the delegate.
    virtual ~Delegate() {}

    // Gets the current time.
    virtual base::Time GetNow() const = 0;

    // Gets the sunset and sunrise times.
    virtual base::Time GetSunsetTime() const = 0;
    virtual base::Time GetSunriseTime() const = 0;

    // Provides the delegate with the geoposition so that it can be used to
    // calculate sunset and sunrise times.
    virtual void SetGeoposition(mojom::SimpleGeopositionPtr position) = 0;
  };

  class Observer {
   public:
    // Emitted when the NightLight status is changed.
    virtual void OnNightLightEnabledChanged(bool enabled) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit NightLightController(SessionController* session_controller);
  ~NightLightController() override;

  // Returns true if the NightLight feature is enabled in the flags.
  static bool IsFeatureEnabled();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  AnimationDuration animation_duration() const { return animation_duration_; }
  AnimationDuration last_animation_duration() const {
    return last_animation_duration_;
  }
  const base::OneShotTimer& timer() const { return timer_; }

  void BindRequest(mojom::NightLightControllerRequest request);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the NightLight settings stored in the current active user prefs.
  bool GetEnabled() const;
  float GetColorTemperature() const;
  ScheduleType GetScheduleType() const;
  TimeOfDay GetCustomStartTime() const;
  TimeOfDay GetCustomEndTime() const;

  // Set the desired NightLight settings in the current active user prefs.
  void SetEnabled(bool enabled, AnimationDuration animation_type);
  void SetColorTemperature(float temperature);
  void SetScheduleType(ScheduleType type);
  void SetCustomStartTime(TimeOfDay start_time);
  void SetCustomEndTime(TimeOfDay end_time);

  // This is always called as a result of a user action and will always use the
  // AnimationDurationType::kShort.
  void Toggle();

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // ash::mojom::NightLightController:
  void SetCurrentGeoposition(mojom::SimpleGeopositionPtr position) override;
  void SetClient(mojom::NightLightClientPtr client) override;

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

 private:
  void RefreshLayersTemperature();

  void StartWatchingPrefsChanges();

  void InitFromUserPrefs();

  void NotifyStatusChanged();

  void NotifyClientWithScheduleChange();

  // Called when the user pref for the enabled status of NightLight is changed.
  void OnEnabledPrefChanged();

  // Called when the user pref for the color temperature is changed.
  void OnColorTemperaturePrefChanged();

  // Called when the user pref for the schedule type is changed.
  void OnScheduleTypePrefChanged();

  // Called when either of the custom schedule prefs (custom start or end times)
  // are changed.
  void OnCustomSchedulePrefsChanged();

  // Refreshes the state of NightLight according to the currently set
  // parameters. |did_schedule_change| is true when Refresh() is called as a
  // result of a change in one of the schedule related prefs, and false
  // otherwise.
  void Refresh(bool did_schedule_change);

  // Given the desired start and end times that determine the time interval
  // during which NightLight will be ON, depending on the time of "now", it
  // refreshes the |timer_| to either schedule the future start or end of
  // NightLight mode, as well as update the current status if needed.
  // For |did_schedule_change|, see Refresh() above.
  // This function should never be called if the schedule type is |kNone|.
  void RefreshScheduleTimer(base::Time start_time,
                            base::Time end_time,
                            bool did_schedule_change);

  // Schedule the upcoming next toggle of NightLight mode. This is used for the
  // automatic status changes of NightLight which always use an
  // AnimationDurationType::kLong.
  void ScheduleNextToggle(base::TimeDelta delay);

  // The observed session controller instance from which we know when to
  // initialize the NightLight settings from the user preferences.
  SessionController* const session_controller_;

  std::unique_ptr<Delegate> delegate_;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  PrefService* active_user_pref_service_ = nullptr;

  // The animation duration of any upcoming future change.
  AnimationDuration animation_duration_ = AnimationDuration::kShort;
  // The animation duration of the change that was just performed.
  AnimationDuration last_animation_duration_ = AnimationDuration::kShort;

  // The timer that schedules the start and end of NightLight when the schedule
  // type is either kSunsetToSunrise or kCustom.
  base::OneShotTimer timer_;

  // The registrar used to watch NightLight prefs changes in the above
  // |active_user_pref_service_| from outside ash.
  // NOTE: Prefs are how Chrome communicates changes to the NightLight settings
  // controlled by this class from the WebUI settings.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ObserverList<Observer> observers_;

  mojo::Binding<mojom::NightLightController> binding_;

  mojom::NightLightClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(NightLightController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_CONTROLLER_H_
