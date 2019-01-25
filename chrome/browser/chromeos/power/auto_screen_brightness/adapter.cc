// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

Adapter::Params::Params() {}

Adapter::Adapter(Profile* profile,
                 AlsReader* als_reader,
                 BrightnessMonitor* brightness_monitor,
                 Modeller* modeller,
                 MetricsReporter* metrics_reporter,
                 chromeos::PowerManagerClient* power_manager_client)
    : profile_(profile),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      modeller_observer_(this),
      power_manager_client_observer_(this),
      metrics_reporter_(metrics_reporter),
      power_manager_client_(power_manager_client),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {
  DCHECK(profile);
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(modeller);
  DCHECK(power_manager_client);

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  modeller_observer_.Add(modeller);
  power_manager_client_observer_.Add(power_manager_client);

  power_manager_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&Adapter::OnPowerManagerServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!base::FeatureList::IsEnabled(features::kAutoScreenBrightness)) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  InitParams();
}

Adapter::~Adapter() = default;

void Adapter::OnAmbientLightUpdated(int lux) {
  if (adapter_status_ == Status::kDisabled)
    return;

  const base::TimeTicks now = tick_clock_->NowTicks();

  if (first_als_time_.is_null())
    first_als_time_ = now;

  latest_als_time_ = now;

  ambient_light_values_->SaveToBuffer(
      {params_.average_log_als ? ConvertToLog(lux) : lux, now});

  MaybeAdjustBrightness(now);
}

void Adapter::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  DCHECK(!als_init_status_);

  als_init_status_ = status;
  UpdateStatus();
}

void Adapter::OnBrightnessMonitorInitialized(bool success) {
  DCHECK(!brightness_monitor_success_.has_value());

  brightness_monitor_success_ = success;
  UpdateStatus();
}

void Adapter::OnUserBrightnessChanged(double old_brightness_percent,
                                      double new_brightness_percent) {}

void Adapter::OnUserBrightnessChangeRequested() {
  if (params_.user_adjustment_effect != UserAdjustmentEffect::kContinueAuto) {
    // Adapter will stop making brightness adjustment until suspend/resume or
    // when browser restarts.
    adapter_disabled_by_user_adjustment_ = true;
  }

  if (!als_init_status_)
    return;

  if (!metrics_reporter_)
    return;

  switch (*als_init_status_) {
    case AlsReader::AlsInitStatus::kSuccess:
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kSupportedAls);
      return;
    case AlsReader::AlsInitStatus::kDisabled:
    case AlsReader::AlsInitStatus::kMissingPath:
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kNoAls);
      return;
    case AlsReader::AlsInitStatus::kIncorrectConfig:
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kUnsupportedAls);
      return;
    case AlsReader::AlsInitStatus::kInProgress:
      NOTREACHED() << "ALS should have been initialized with a valid value.";
  }
}

void Adapter::OnModelTrained(const MonotoneCubicSpline& brightness_curve) {
  if (adapter_status_ == Status::kDisabled)
    return;

  personal_curve_.emplace(brightness_curve);
}

void Adapter::OnModelInitialized(
    const base::Optional<MonotoneCubicSpline>& global_curve,
    const base::Optional<MonotoneCubicSpline>& personal_curve) {
  DCHECK(!model_initialized_);

  model_initialized_ = true;

  if (global_curve)
    global_curve_.emplace(*global_curve);
  if (personal_curve)
    personal_curve_.emplace(*personal_curve);

  UpdateStatus();
}

void Adapter::SuspendDone(const base::TimeDelta& /* sleep_duration */) {
  if (params_.user_adjustment_effect == UserAdjustmentEffect::kPauseAuto)
    adapter_disabled_by_user_adjustment_ = false;
}

void Adapter::SetTickClockForTesting(const base::TickClock* test_tick_clock) {
  tick_clock_ = test_tick_clock;
}

Adapter::Status Adapter::GetStatusForTesting() const {
  return adapter_status_;
}

bool Adapter::IsAppliedForTesting() const {
  return (adapter_status_ == Status::kSuccess &&
          !adapter_disabled_by_user_adjustment_);
}

base::Optional<MonotoneCubicSpline> Adapter::GetGlobalCurveForTesting() const {
  return global_curve_;
}

base::Optional<MonotoneCubicSpline> Adapter::GetPersonalCurveForTesting()
    const {
  return personal_curve_;
}

base::Optional<double> Adapter::GetAverageAmbientForTesting(
    base::TimeTicks now) {
  return ambient_light_values_->AverageAmbient(now);
}

double Adapter::GetBrighteningThresholdForTesting() const {
  return *brightening_lux_threshold_;
}

double Adapter::GetDarkeningThresholdForTesting() const {
  return *darkening_lux_threshold_;
}

void Adapter::InitParams() {
  params_.brightening_lux_threshold_ratio = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightening_lux_threshold_ratio",
      params_.brightening_lux_threshold_ratio);
  if (params_.brightening_lux_threshold_ratio <= 0) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.darkening_lux_threshold_ratio = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "darkening_lux_threshold_ratio",
      params_.darkening_lux_threshold_ratio);
  if (params_.darkening_lux_threshold_ratio <= 0 ||
      params_.darkening_lux_threshold_ratio > 1) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.immediate_brightening_lux_threshold_ratio =
      GetFieldTrialParamByFeatureAsDouble(
          features::kAutoScreenBrightness,
          "immediate_brightening_lux_threshold_ratio",
          params_.immediate_brightening_lux_threshold_ratio);
  if (params_.immediate_brightening_lux_threshold_ratio <
      params_.brightening_lux_threshold_ratio) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.immediate_darkening_lux_threshold_ratio =
      GetFieldTrialParamByFeatureAsDouble(
          features::kAutoScreenBrightness,
          "immediate_darkening_lux_threshold_ratio",
          params_.immediate_darkening_lux_threshold_ratio);
  if (params_.immediate_darkening_lux_threshold_ratio <
          params_.darkening_lux_threshold_ratio ||
      params_.immediate_darkening_lux_threshold_ratio > 1) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.update_brightness_on_startup = GetFieldTrialParamByFeatureAsBool(
      features::kAutoScreenBrightness, "update_brightness_on_startup",
      params_.update_brightness_on_startup);

  const int min_seconds_between_brightness_changes =
      GetFieldTrialParamByFeatureAsInt(
          features::kAutoScreenBrightness,
          "min_seconds_between_brightness_changes",
          params_.min_time_between_brightness_changes.InSeconds());
  if (min_seconds_between_brightness_changes < 0) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.min_time_between_brightness_changes =
      base::TimeDelta::FromSeconds(min_seconds_between_brightness_changes);

  const int model_curve = base::GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "model_curve", 2);
  if (model_curve < 0 || model_curve > 2) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.model_curve = static_cast<ModelCurve>(model_curve);

  const int auto_brightness_als_horizon_seconds =
      GetFieldTrialParamByFeatureAsInt(
          features::kAutoScreenBrightness,
          "auto_brightness_als_horizon_seconds",
          params_.auto_brightness_als_horizon.InSeconds());

  if (auto_brightness_als_horizon_seconds <= 0) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.auto_brightness_als_horizon =
      base::TimeDelta::FromSeconds(auto_brightness_als_horizon_seconds);
  ambient_light_values_ = std::make_unique<AmbientLightSampleBuffer>(
      params_.auto_brightness_als_horizon);

  params_.average_log_als = GetFieldTrialParamByFeatureAsBool(
      features::kAutoScreenBrightness, "average_log_als",
      params_.average_log_als);

  const int user_adjustment_effect_as_int = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "user_adjustment_effect",
      static_cast<int>(params_.user_adjustment_effect));
  if (user_adjustment_effect_as_int < 0 || user_adjustment_effect_as_int > 2) {
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.user_adjustment_effect =
      static_cast<UserAdjustmentEffect>(user_adjustment_effect_as_int);

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.UserAdjustmentEffect",
                            params_.user_adjustment_effect);
}

void Adapter::OnPowerManagerServiceAvailable(bool service_is_ready) {
  power_manager_service_available_ = service_is_ready;
  UpdateStatus();
}

void Adapter::UpdateStatus() {
  if (adapter_status_ != Status::kInitializing)
    return;

  if (!als_init_status_)
    return;

  const bool als_success =
      *als_init_status_ == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!brightness_monitor_success_.has_value())
    return;

  if (!*brightness_monitor_success_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!model_initialized_)
    return;

  if (!global_curve_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!power_manager_service_available_.has_value())
    return;

  if (!*power_manager_service_available_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  adapter_status_ = Status::kSuccess;
}

base::Optional<Adapter::BrightnessChangeCause> Adapter::CanAdjustBrightness(
    double current_average_ambient) const {
  if (adapter_status_ != Status::kSuccess ||
      adapter_disabled_by_user_adjustment_)
    return base::nullopt;

  // Do not change brightness if it's set by the policy, but do not completely
  // disable the model as the policy could change.
  if (profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerAcScreenBrightnessPercent) >= 0 ||
      profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerBatteryScreenBrightnessPercent) >= 0) {
    return base::nullopt;
  }

  if (latest_brightness_change_time_.is_null()) {
    // Brightness hasn't been changed before.
    const bool can_adjust_brightness =
        latest_als_time_ - first_als_time_ >=
            params_.auto_brightness_als_horizon ||
        params_.update_brightness_on_startup;

    if (can_adjust_brightness)
      return BrightnessChangeCause::kInitialAlsReceived;

    return base::nullopt;
  }

  // The following thresholds should have been set last time when brightness was
  // changed.
  if (current_average_ambient > *immediate_brightening_lux_threshold_) {
    return BrightnessChangeCause::kImmediateBrightneningThresholdExceeded;
  }

  if (current_average_ambient < *immediate_darkening_lux_threshold_) {
    return BrightnessChangeCause::kImmediateDarkeningThresholdExceeded;
  }

  if (tick_clock_->NowTicks() - latest_brightness_change_time_ <
      params_.min_time_between_brightness_changes) {
    return base::nullopt;
  }

  if (current_average_ambient > *brightening_lux_threshold_) {
    return BrightnessChangeCause::kBrightneningThresholdExceeded;
  }

  if (current_average_ambient < *darkening_lux_threshold_) {
    return BrightnessChangeCause::kDarkeningThresholdExceeded;
  }

  return base::nullopt;
}

void Adapter::MaybeAdjustBrightness(base::TimeTicks now) {
  const base::Optional<double> average_ambient_lux_opt =
      ambient_light_values_->AverageAmbient(now);
  if (!average_ambient_lux_opt)
    return;

  const double average_ambient_lux = average_ambient_lux_opt.value();

  const base::Optional<BrightnessChangeCause> can_adjust_brightness =
      CanAdjustBrightness(average_ambient_lux);

  if (!can_adjust_brightness.has_value())
    return;

  // If |params_.average_log_als| is true, then |average_ambient_lux| is
  // the average of log-lux. Hence we don't need to convert it into log space
  // again.
  const base::Optional<double> brightness = GetBrightnessBasedOnAmbientLogLux(
      params_.average_log_als ? average_ambient_lux
                              : ConvertToLog(average_ambient_lux));

  // This could occur if curve isn't set up (e.g. when we want to use
  // personal only that's not yet available).
  if (!brightness)
    return;

  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(*brightness);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL);
  request.set_cause(power_manager::SetBacklightBrightnessRequest_Cause_MODEL);
  power_manager_client_->SetScreenBrightness(request);

  const base::TimeTicks brightness_change_time = tick_clock_->NowTicks();
  if (!latest_brightness_change_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "AutoScreenBrightness.BrightnessChange.ElapsedTime",
        brightness_change_time - latest_brightness_change_time_);
  }
  latest_brightness_change_time_ = brightness_change_time;

  const BrightnessChangeCause cause = *can_adjust_brightness;
  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.BrightnessChange.Cause",
                            cause);

  average_ambient_lux_ = average_ambient_lux;

  UpdateLuxThresholds();
}

void Adapter::UpdateLuxThresholds() {
  DCHECK(average_ambient_lux_);
  DCHECK_GE(*average_ambient_lux_, 0);

  brightening_lux_threshold_ =
      *average_ambient_lux_ * (1 + params_.brightening_lux_threshold_ratio);
  darkening_lux_threshold_ =
      *average_ambient_lux_ * (1 - params_.darkening_lux_threshold_ratio);
  immediate_brightening_lux_threshold_ =
      *average_ambient_lux_ *
      (1 + params_.immediate_brightening_lux_threshold_ratio);
  immediate_darkening_lux_threshold_ =
      *average_ambient_lux_ *
      (1 - params_.immediate_darkening_lux_threshold_ratio);

  DCHECK_GE(*brightening_lux_threshold_, 0);
  DCHECK_GE(*darkening_lux_threshold_, 0);
  DCHECK_GE(*immediate_brightening_lux_threshold_, 0);
  DCHECK_GE(*immediate_darkening_lux_threshold_, 0);
}

base::Optional<double> Adapter::GetBrightnessBasedOnAmbientLogLux(
    double ambient_log_lux) const {
  switch (params_.model_curve) {
    case ModelCurve::kGlobal:
      return global_curve_->Interpolate(ambient_log_lux);
    case ModelCurve::kPersonal:
      if (personal_curve_)
        return personal_curve_->Interpolate(ambient_log_lux);
      return base::nullopt;  // signal brightness shouldn't be changed
    default:
      // We use the latest curve available.
      if (personal_curve_)
        return personal_curve_->Interpolate(ambient_log_lux);
      return global_curve_->Interpolate(ambient_log_lux);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
