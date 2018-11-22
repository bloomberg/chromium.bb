// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

constexpr base::TimeDelta Adapter::kAmbientLightShortHorizon;
constexpr int Adapter::kNumberAmbientValuesToTrack;

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

  ambient_light_values_.SaveToBuffer({lux, now});

  MaybeAdjustBrightness();
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
  // This will disable |adapter_status_| so that the model will not make any
  // brightness adjustment.
  adapter_status_ = Status::kDisabled;
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

void Adapter::SetTickClockForTesting(const base::TickClock* test_tick_clock) {
  tick_clock_ = test_tick_clock;
}

Adapter::Status Adapter::GetStatusForTesting() const {
  return adapter_status_;
}

base::Optional<MonotoneCubicSpline> Adapter::GetGlobalCurveForTesting() const {
  return global_curve_;
}

base::Optional<MonotoneCubicSpline> Adapter::GetPersonalCurveForTesting()
    const {
  return personal_curve_;
}

double Adapter::GetAverageAmbientForTesting() const {
  return AverageAmbient(ambient_light_values_, -1);
}

double Adapter::GetBrighteningThresholdForTesting() const {
  return *brightening_lux_threshold_;
}

double Adapter::GetDarkeningThresholdForTesting() const {
  return *darkening_lux_threshold_;
}

// TODO(jiameng): add UMA metrics to record errors.
void Adapter::InitParams() {
  params_.brightening_lux_threshold_ratio = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightening_lux_threshold_ratio",
      params_.brightening_lux_threshold_ratio);
  if (params_.brightening_lux_threshold_ratio <= 0) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  params_.darkening_lux_threshold_ratio = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "darkening_lux_threshold_ratio",
      params_.darkening_lux_threshold_ratio);
  if (params_.darkening_lux_threshold_ratio <= 0 ||
      params_.darkening_lux_threshold_ratio > 1) {
    adapter_status_ = Status::kDisabled;
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
    return;
  }
  params_.min_time_between_brightness_changes =
      base::TimeDelta::FromSeconds(min_seconds_between_brightness_changes);

  const int model_curve = base::GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "model_curve", 2);
  if (model_curve < 0 || model_curve > 2) {
    adapter_status_ = Status::kDisabled;
    return;
  }
  params_.model_curve = static_cast<ModelCurve>(model_curve);
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

bool Adapter::CanAdjustBrightness(double current_average_ambient) const {
  if (adapter_status_ != Status::kSuccess)
    return false;

  // Do not change brightness if it's set by the policy, but do not completely
  // disable the model as the policy could change.
  if (profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerAcScreenBrightnessPercent) >= 0 ||
      profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerBatteryScreenBrightnessPercent) >= 0) {
    return false;
  }

  if (latest_brightness_change_time_.is_null()) {
    // Brightness hasn't been changed before.
    return latest_als_time_ - first_als_time_ >= kAmbientLightShortHorizon ||
           params_.update_brightness_on_startup;
  }

  // The following thresholds should have been set last time when brightness was
  // changed.
  if (current_average_ambient > *immediate_brightening_lux_threshold_ ||
      current_average_ambient < *immediate_darkening_lux_threshold_) {
    return true;
  }

  if (tick_clock_->NowTicks() - latest_brightness_change_time_ <
      params_.min_time_between_brightness_changes) {
    return false;
  }

  return current_average_ambient > *brightening_lux_threshold_ ||
         current_average_ambient < *darkening_lux_threshold_;
}

void Adapter::MaybeAdjustBrightness() {
  const double average_ambient_lux = AverageAmbient(ambient_light_values_, -1);

  if (!CanAdjustBrightness(average_ambient_lux))
    return;

  const base::Optional<double> brightness =
      GetBrightnessBasedOnAmbientLogLux(ConvertToLog(average_ambient_lux));

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

  latest_brightness_change_time_ = tick_clock_->NowTicks();
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
