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
                 ModelConfigLoader* model_config_loader,
                 MetricsReporter* metrics_reporter,
                 chromeos::PowerManagerClient* power_manager_client)
    : profile_(profile),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      modeller_observer_(this),
      model_config_loader_observer_(this),
      power_manager_client_observer_(this),
      metrics_reporter_(metrics_reporter),
      power_manager_client_(power_manager_client),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {
  DCHECK(profile);
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(modeller);
  DCHECK(model_config_loader);
  DCHECK(power_manager_client);

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  modeller_observer_.Add(modeller);
  model_config_loader_observer_.Add(model_config_loader);
  power_manager_client_observer_.Add(power_manager_client);

  power_manager_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&Adapter::OnPowerManagerServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

Adapter::~Adapter() = default;

void Adapter::OnAmbientLightUpdated(int lux) {
  if (adapter_status_ != Status::kSuccess)
    return;

  DCHECK(ambient_light_values_);

  const base::TimeTicks now = tick_clock_->NowTicks();

  ambient_light_values_->SaveToBuffer({lux, now});

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
      DCHECK(!params_.metrics_key.empty());
      if (params_.metrics_key == "eve") {
        metrics_reporter_->OnUserBrightnessChangeRequested(
            MetricsReporter::UserAdjustment::kEve);
        return;
      }
      if (params_.metrics_key == "atlas") {
        metrics_reporter_->OnUserBrightnessChangeRequested(
            MetricsReporter::UserAdjustment::kAtlas);
        return;
      }
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

void Adapter::OnModelConfigLoaded(base::Optional<ModelConfig> model_config) {
  DCHECK(!model_config_exists_.has_value());

  model_config_exists_ = model_config.has_value();

  if (model_config_exists_.value()) {
    InitParams(model_config.value());
  }

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
  DCHECK(ambient_light_values_);
  const base::Optional<double> avg = ambient_light_values_->AverageAmbient(now);
  if (!avg)
    return base::nullopt;

  return ConvertToLog(avg.value());
}

double Adapter::GetBrighteningThresholdForTesting() const {
  return *brightening_threshold_;
}

double Adapter::GetDarkeningThresholdForTesting() const {
  return *darkening_threshold_;
}

void Adapter::InitParams(const ModelConfig& model_config) {
  if (!base::FeatureList::IsEnabled(features::kAutoScreenBrightness) &&
      model_config.metrics_key != "atlas") {
    // TODO(jiameng): eventually we will control which device has adapter
    // enabled by finch. For now, atlas will always be enabled. We'll change the
    // fixed behaviour when finch experiment for it is set up.
    adapter_status_ = Status::kDisabled;
    return;
  }

  params_.metrics_key = model_config.metrics_key;
  params_.brightening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightening_log_lux_threshold",
      params_.brightening_log_lux_threshold);

  params_.darkening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "darkening_log_lux_threshold",
      params_.darkening_log_lux_threshold);

  const int model_curve = base::GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "model_curve", 2);
  if (model_curve < 0 || model_curve > 2) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.model_curve = static_cast<ModelCurve>(model_curve);

  const int auto_brightness_als_horizon_seconds =
      model_config.auto_brightness_als_horizon_seconds;

  if (auto_brightness_als_horizon_seconds <= 0) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.auto_brightness_als_horizon =
      base::TimeDelta::FromSeconds(auto_brightness_als_horizon_seconds);
  ambient_light_values_ = std::make_unique<AmbientLightSampleBuffer>(
      params_.auto_brightness_als_horizon);

  // TODO(jiameng): move this to device config once we complete experiments.
  if (model_config.metrics_key == "atlas") {
    params_.user_adjustment_effect = UserAdjustmentEffect::kContinueAuto;
  }

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

  if (!model_config_exists_.has_value())
    return;

  if (!model_config_exists_.value()) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  adapter_status_ = Status::kSuccess;
}

base::Optional<Adapter::BrightnessChangeCause> Adapter::CanAdjustBrightness(
    double current_log_average_ambient) const {
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
    return BrightnessChangeCause::kInitialAlsReceived;
  }

  // The following thresholds should have been set last time when brightness was
  // changed.
  if (current_log_average_ambient > *brightening_threshold_) {
    return BrightnessChangeCause::kBrightneningThresholdExceeded;
  }

  if (current_log_average_ambient < *darkening_threshold_) {
    return BrightnessChangeCause::kDarkeningThresholdExceeded;
  }

  return base::nullopt;
}

void Adapter::MaybeAdjustBrightness(base::TimeTicks now) {
  DCHECK(ambient_light_values_);
  const base::Optional<double> average_ambient_lux_opt =
      ambient_light_values_->AverageAmbient(now);
  if (!average_ambient_lux_opt)
    return;

  const double log_average_ambient_lux =
      ConvertToLog(average_ambient_lux_opt.value());

  const base::Optional<BrightnessChangeCause> can_adjust_brightness =
      CanAdjustBrightness(log_average_ambient_lux);

  if (!can_adjust_brightness.has_value())
    return;

  const base::Optional<double> brightness =
      GetBrightnessBasedOnAmbientLogLux(log_average_ambient_lux);

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

  log_average_ambient_lux_ = log_average_ambient_lux;

  UpdateBrightnessChangeThresholds();
}

void Adapter::UpdateBrightnessChangeThresholds() {
  DCHECK(log_average_ambient_lux_);

  brightening_threshold_ =
      *log_average_ambient_lux_ + params_.brightening_log_lux_threshold;
  darkening_threshold_ =
      *log_average_ambient_lux_ - params_.darkening_log_lux_threshold;
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
