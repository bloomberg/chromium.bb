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
#include "third_party/icu/source/i18n/astro.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

// Default start time at 6:00 PM as an offset from 00:00.
const int kDefaultStartTimeOffsetMinutes = 18 * 60;

// Default end time at 6:00 AM as an offset from 00:00.
const int kDefaultEndTimeOffsetMinutes = 6 * 60;

constexpr float kDefaultColorTemperature = 0.5f;

// The duration of the temperature change animation for
// AnimationDurationType::kShort.
constexpr int kManualAnimationDurationSec = 2;

// The duration of the temperature change animation for
// AnimationDurationType::kLong.
constexpr int kAutomaticAnimationDurationSec = 20;

class NightLightControllerDelegateImpl : public NightLightController::Delegate {
 public:
  NightLightControllerDelegateImpl() = default;
  ~NightLightControllerDelegateImpl() override = default;

  // ash::NightLightController::Delegate:
  base::Time GetNow() const override { return base::Time::Now(); }
  base::Time GetSunsetTime() const override { return GetSunRiseSet(false); }
  base::Time GetSunriseTime() const override { return GetSunRiseSet(true); }
  void SetGeoposition(mojom::SimpleGeopositionPtr position) override {
    position_ = std::move(position);
  }

 private:
  base::Time GetSunRiseSet(bool sunrise) const {
    if (!ValidatePosition()) {
      return sunrise ? TimeOfDay(kDefaultEndTimeOffsetMinutes).ToTimeToday()
                     : TimeOfDay(kDefaultStartTimeOffsetMinutes).ToTimeToday();
    }

    icu::CalendarAstronomer astro(position_->longitude, position_->latitude);
    // For sunset and sunrise times calculations to be correct, the time of the
    // icu::CalendarAstronomer object should be set to a time near local noon.
    // This avoids having the computation flopping over into an adjacent day.
    // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
    // Note that the icu calendar works with milliseconds since epoch, and
    // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
    const double noon_today_sec = TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
    astro.setTime(noon_today_sec * 1000.0);
    const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
    return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
  }

  bool ValidatePosition() const { return !!position_; }

  mojom::SimpleGeopositionPtr position_;

  DISALLOW_COPY_AND_ASSIGN(NightLightControllerDelegateImpl);
};

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
    : session_controller_(session_controller),
      delegate_(base::MakeUnique<NightLightControllerDelegateImpl>()),
      binding_(this) {
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
  registry->RegisterIntegerPref(prefs::kNightLightScheduleType,
                                static_cast<int>(ScheduleType::kNone));
  registry->RegisterIntegerPref(prefs::kNightLightCustomStartTime,
                                kDefaultStartTimeOffsetMinutes);
  registry->RegisterIntegerPref(prefs::kNightLightCustomEndTime,
                                kDefaultEndTimeOffsetMinutes);
}

void NightLightController::BindRequest(
    mojom::NightLightControllerRequest request) {
  binding_.Bind(std::move(request));
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

NightLightController::ScheduleType NightLightController::GetScheduleType()
    const {
  if (active_user_pref_service_) {
    return static_cast<ScheduleType>(
        active_user_pref_service_->GetInteger(prefs::kNightLightScheduleType));
  }

  return ScheduleType::kNone;
}

TimeOfDay NightLightController::GetCustomStartTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(active_user_pref_service_->GetInteger(
        prefs::kNightLightCustomStartTime));
  }

  return TimeOfDay(kDefaultStartTimeOffsetMinutes);
}

TimeOfDay NightLightController::GetCustomEndTime() const {
  if (active_user_pref_service_) {
    return TimeOfDay(
        active_user_pref_service_->GetInteger(prefs::kNightLightCustomEndTime));
  }

  return TimeOfDay(kDefaultEndTimeOffsetMinutes);
}

void NightLightController::SetEnabled(bool enabled,
                                      AnimationDuration animation_type) {
  if (active_user_pref_service_) {
    animation_duration_ = animation_type;
    active_user_pref_service_->SetBoolean(prefs::kNightLightEnabled, enabled);
  }
}

void NightLightController::SetColorTemperature(float temperature) {
  DCHECK_GE(temperature, 0.0f);
  DCHECK_LE(temperature, 1.0f);
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(prefs::kNightLightTemperature,
                                         temperature);
  }
}

void NightLightController::SetScheduleType(ScheduleType type) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(prefs::kNightLightScheduleType,
                                          static_cast<int>(type));
  }
}

void NightLightController::SetCustomStartTime(TimeOfDay start_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomStartTime,
        start_time.offset_minutes_from_zero_hour());
  }
}

void NightLightController::SetCustomEndTime(TimeOfDay end_time) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetInteger(
        prefs::kNightLightCustomEndTime,
        end_time.offset_minutes_from_zero_hour());
  }
}

void NightLightController::Toggle() {
  SetEnabled(!GetEnabled(), AnimationDuration::kShort);
}

void NightLightController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Initial login and user switching in multi profiles.
  pref_change_registrar_.reset();
  active_user_pref_service_ = Shell::Get()->GetActiveUserPrefService();
  InitFromUserPrefs();
}

void NightLightController::SetCurrentGeoposition(
    mojom::SimpleGeopositionPtr position) {
  delegate_->SetGeoposition(std::move(position));

  // If the schedule type is sunset to sunrise, then a potential change in the
  // geoposition might mean a change in the start and end times. In this case,
  // we must trigger a refresh.
  if (GetScheduleType() == ScheduleType::kSunsetToSunrise)
    Refresh(true /* did_schedule_change */);
}

void NightLightController::SetClient(mojom::NightLightClientPtr client) {
  client_ = std::move(client);

  if (client_) {
    client_.set_connection_error_handler(base::Bind(
        &NightLightController::SetClient, base::Unretained(this), nullptr));
    NotifyClientWithScheduleChange();
  }
}

void NightLightController::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void NightLightController::RefreshLayersTemperature() {
  ApplyColorTemperatureToLayers(
      GetEnabled() ? GetColorTemperature() : 0.0f,
      base::TimeDelta::FromSeconds(animation_duration_ ==
                                           AnimationDuration::kShort
                                       ? kManualAnimationDurationSec
                                       : kAutomaticAnimationDurationSec));

  // Reset the animation type back to manual to consume any automatically set
  // animations.
  last_animation_duration_ = animation_duration_;
  animation_duration_ = AnimationDuration::kShort;
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
  pref_change_registrar_->Add(
      prefs::kNightLightScheduleType,
      base::Bind(&NightLightController::OnScheduleTypePrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomStartTime,
      base::Bind(&NightLightController::OnCustomSchedulePrefsChanged,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kNightLightCustomEndTime,
      base::Bind(&NightLightController::OnCustomSchedulePrefsChanged,
                 base::Unretained(this)));
}

void NightLightController::InitFromUserPrefs() {
  if (!active_user_pref_service_) {
    // The pref_service can be null in ash_unittests.
    return;
  }

  StartWatchingPrefsChanges();
  Refresh(true /* did_schedule_change */);
  NotifyStatusChanged();
  NotifyClientWithScheduleChange();
}

void NightLightController::NotifyStatusChanged() {
  for (auto& observer : observers_)
    observer.OnNightLightEnabledChanged(GetEnabled());
}

void NightLightController::NotifyClientWithScheduleChange() {
  if (client_)
    client_->OnScheduleTypeChanged(GetScheduleType());
}

void NightLightController::OnEnabledPrefChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(false /* did_schedule_change */);
  NotifyStatusChanged();
}

void NightLightController::OnColorTemperaturePrefChanged() {
  DCHECK(active_user_pref_service_);
  RefreshLayersTemperature();
}

void NightLightController::OnScheduleTypePrefChanged() {
  DCHECK(active_user_pref_service_);
  NotifyClientWithScheduleChange();
  Refresh(true /* did_schedule_change */);
}

void NightLightController::OnCustomSchedulePrefsChanged() {
  DCHECK(active_user_pref_service_);
  Refresh(true /* did_schedule_change */);
}

void NightLightController::Refresh(bool did_schedule_change) {
  RefreshLayersTemperature();

  const ScheduleType type = GetScheduleType();
  switch (type) {
    case ScheduleType::kNone:
      timer_.Stop();
      return;

    case ScheduleType::kSunsetToSunrise:
      RefreshScheduleTimer(delegate_->GetSunsetTime(),
                           delegate_->GetSunriseTime(), did_schedule_change);
      return;

    case ScheduleType::kCustom:
      RefreshScheduleTimer(GetCustomStartTime().ToTimeToday(),
                           GetCustomEndTime().ToTimeToday(),
                           did_schedule_change);
      return;
  }
}

void NightLightController::RefreshScheduleTimer(base::Time start_time,
                                                base::Time end_time,
                                                bool did_schedule_change) {
  if (GetScheduleType() == ScheduleType::kNone) {
    NOTREACHED();
    timer_.Stop();
    return;
  }

  // NOTE: Users can set any weird combinations.
  if (end_time <= start_time) {
    // Example:
    // Start: 9:00 PM, End: 6:00 AM.
    //
    //       6:00                21:00
    // <----- + ------------------ + ----->
    //        |                    |
    //       end                 start
    //
    // From our perspective, the end time is always a day later.
    end_time += base::TimeDelta::FromDays(1);
  }

  DCHECK_GE(end_time, start_time);

  // The target status that we need to set NightLight to now if a change of
  // status is needed immediately.
  bool enable_now = false;

  // Where are we now with respect to the start and end times?
  const base::Time now = delegate_->GetNow();
  if (now < start_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 4:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //       now          start          end
    //
    // In this case, we need to disable NightLight immediately if it's enabled.
    enable_now = false;
  } else if (now >= start_time && now < end_time) {
    // Example:
    // Start: 6:00 PM today, End: 6:00 AM tomorrow, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          now           end
    //
    // Start NightLight right away. Our future start time is a day later than
    // its current value.
    enable_now = true;
    start_time += base::TimeDelta::FromDays(1);
  } else {  // now >= end_time.
    // Example:
    // Start: 6:00 PM today, End: 10:00 PM today, Now: 11:00 PM.
    //
    // <----- + ----------- + ----------- + ----->
    //        |             |             |
    //      start          end           now
    //
    // In this case, our future start and end times are a day later from their
    // current values. NightLight needs to be ended immediately if it's already
    // enabled.
    enable_now = false;
    start_time += base::TimeDelta::FromDays(1);
    end_time += base::TimeDelta::FromDays(1);
  }

  // After the above processing, the start and end time are all in the future.
  DCHECK_GE(start_time, now);
  DCHECK_GE(end_time, now);

  if (did_schedule_change && enable_now != GetEnabled()) {
    // If the change in the schedule introduces a change in the status, then
    // calling SetEnabled() is all we need, since it will trigger a change in
    // the user prefs to which we will respond by calling Refresh(). This will
    // end up in this function again, adjusting all the needed schedules.
    SetEnabled(enable_now, AnimationDuration::kShort);
    return;
  }

  // We reach here in one of the following conditions:
  // 1) If schedule changes don't result in changes in the status, we need to
  // explicitly update the timer to re-schedule the next toggle to account for
  // any changes.
  // 2) The user has just manually toggled the status of NightLight either from
  // the System Menu or System Settings. In this case, we respect the user
  // wish and maintain the current status that he desires, but we schedule the
  // status to be toggled according to the time that corresponds with the
  // opposite status of the current one.
  ScheduleNextToggle(GetEnabled() ? end_time - now : start_time - now);
}

void NightLightController::ScheduleNextToggle(base::TimeDelta delay) {
  timer_.Start(
      FROM_HERE, delay,
      base::Bind(&NightLightController::SetEnabled, base::Unretained(this),
                 !GetEnabled(), AnimationDuration::kLong));
}

}  // namespace ash
