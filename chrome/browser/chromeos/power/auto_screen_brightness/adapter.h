// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_samples.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chromeos/dbus/power_manager_client.h"

class Profile;

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Adapter monitors changes in ambient light, selects an optimal screen
// brightness as predicted by the model and instructs powerd to change it.
class Adapter : public AlsReader::Observer,
                public BrightnessMonitor::Observer,
                public Modeller::Observer,
                public PowerManagerClient::Observer {
 public:
  // Type of curve to use.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ModelCurve {
    // Always use the global curve.
    kGlobal = 0,
    // Always use the personal curve, and make no brightness adjustment until a
    // personal curve is trained.
    kPersonal = 1,
    // Use the personal curve if available, else use the global curve.
    kLatest = 2,
    kMaxValue = kLatest
  };

  // How user manual brightness change will affect Adapter.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UserAdjustmentEffect {
    // Completely disable Adapter until browser restarts.
    kDisableAuto = 0,
    // Pause Adapter until system is suspended and then resumed.
    kPauseAuto = 1,
    // No impact on Adapter and Adapter continues to auto-adjust brightness.
    kContinueAuto = 2,
    kMaxValue = kContinueAuto
  };

  // The values in Params can be overridden by experiment flags.
  struct Params {
    Params();

    // Average ambient value has to go up (resp. down) by
    // |brightening_lux_threshold_ratio| (resp. |darkening_lux_threshold_ratio|)
    // from the current value before brightness could be changed: brightness
    // will actually be changed if |min_time_between_brightness_changes| has
    // passed from the previous change.
    double brightening_lux_threshold_ratio = 0.3;
    double darkening_lux_threshold_ratio = 0.4;

    // If average ambient value changes by more than the "immediate" thresholds
    // then brightness transition will happen immediately, without waiting for
    // |min_time_between_brightness_changes| to elapse. This value should be
    // greater than or equal to the max of brightening/darkening thresholds
    // above.
    double immediate_brightening_lux_threshold_ratio = 0.4;
    double immediate_darkening_lux_threshold_ratio = 0.5;

    // Whether brightness should be set to the predicted value when the first
    // ambient reading comes in. If false, we'll wait for
    // |auto_brightness_als_horizon| of ambient values before setting brightness
    // for the first time.
    bool update_brightness_on_startup = true;

    // Once a brightness change occurs, the next one will not occur until at
    // least |min_time_between_brightness_changes| later, unless ambient change
    // exceeds |immediate_brightening_lux_threshold_ratio| or
    // |immediate_darkening_lux_threshold_ratio|.
    base::TimeDelta min_time_between_brightness_changes =
        base::TimeDelta::FromSeconds(10);

    ModelCurve model_curve = ModelCurve::kLatest;

    // Average ambient value is calculated over the past
    // |auto_brightness_als_horizon|. This is only used for brightness update,
    // which can be different from the horizon used in model training.
    base::TimeDelta auto_brightness_als_horizon =
        base::TimeDelta::FromSeconds(5);

    // If true, we take logs of lux values before averaging. If false, we take
    // logs of averaged lux values. This should be the same as
    // that used by the modeller.
    bool average_log_als = false;

    UserAdjustmentEffect user_adjustment_effect =
        UserAdjustmentEffect::kDisableAuto;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kInitializing = 0,
    kSuccess = 1,
    kDisabled = 2,
    kMaxValue = kDisabled
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BrightnessChangeCause {
    kInitialAlsReceived = 0,
    kImmediateBrightneningThresholdExceeded = 1,
    kImmediateDarkeningThresholdExceeded = 2,
    kBrightneningThresholdExceeded = 3,
    kDarkeningThresholdExceeded = 4,
    kMaxValue = kDarkeningThresholdExceeded
  };

  Adapter(Profile* profile,
          AlsReader* als_reader,
          BrightnessMonitor* brightness_monitor,
          Modeller* modeller,
          MetricsReporter* metrics_reporter,
          chromeos::PowerManagerClient* power_manager_client);
  ~Adapter() override;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(int lux) override;
  void OnAlsReaderInitialized(AlsReader::AlsInitStatus status) override;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override;
  void OnUserBrightnessChanged(double old_brightness_percent,
                               double new_brightness_percent) override;
  void OnUserBrightnessChangeRequested() override;

  // Modeller::Observer overrides:
  void OnModelTrained(const MonotoneCubicSpline& brightness_curve) override;
  void OnModelInitialized(
      const base::Optional<MonotoneCubicSpline>& global_curve,
      const base::Optional<MonotoneCubicSpline>& personal_curve) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  void SetTickClockForTesting(const base::TickClock* test_tick_clock);

  Status GetStatusForTesting() const;

  // Only returns true if Adapter status is success and it's not disabled by
  // user adjustment.
  bool IsAppliedForTesting() const;
  base::Optional<MonotoneCubicSpline> GetGlobalCurveForTesting() const;
  base::Optional<MonotoneCubicSpline> GetPersonalCurveForTesting() const;

  // Returns the actual average over |ambient_light_values_|, which is not
  // necessarily the same as |average_ambient_lux_|.
  base::Optional<double> GetAverageAmbientForTesting(base::TimeTicks now);
  double GetBrighteningThresholdForTesting() const;
  double GetDarkeningThresholdForTesting() const;

 private:
  // Called by the constructor to initialize |params_| possibly from experiment
  // flags. It will disable the adapter if any param value is invalid.
  void InitParams();

  // Called when powerd becomes available.
  void OnPowerManagerServiceAvailable(bool service_is_ready);

  // Called to update |adapter_status_| when there's some status change from
  // AlsReader, BrightnessMonitor, Modeller or power manager.
  void UpdateStatus();

  // Returns a BrightnessChangeCause if the adapter can change the brightness.
  // This is generally the case when the brightness hasn't been manually
  // set, we've received enough initial ambient light readings, and
  // the ambient light has changed beyond thresholds for a long enough
  // period of time.
  // Returns nullopt if it shouldn't change the brightness.
  base::Optional<BrightnessChangeCause> CanAdjustBrightness(
      double current_average_ambient) const;

  // Called when ambient light changes. It only changes screen brightness if
  // |CanAdjustBrightness| returns true and a required curve is set up:
  // if the required curve is personal but no personal curve is available, then
  // brightness won't be changed.
  // It will call |UpdateLuxThresholds| if brightness is actually changed.
  void MaybeAdjustBrightness(base::TimeTicks now);

  // This is only called when brightness is changed.
  void UpdateLuxThresholds();

  // Calculates brightness from given |ambient_lux| based on either
  // |global_curve_| or |personal_curve_| (as specified by the experiment
  // params). Returns nullopt if a personal curve should be used but it's not
  // available.
  base::Optional<double> GetBrightnessBasedOnAmbientLogLux(
      double ambient_lux) const;

  Profile* const profile_;

  ScopedObserver<AlsReader, AlsReader::Observer> als_reader_observer_;
  ScopedObserver<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observer_;
  ScopedObserver<Modeller, Modeller::Observer> modeller_observer_;

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  // Used to report daily metrics to UMA. This may be null in unit tests.
  MetricsReporter* metrics_reporter_;

  chromeos::PowerManagerClient* const power_manager_client_;

  Params params_;

  // This will be replaced by a mock tick clock during tests.
  const base::TickClock* tick_clock_;

  // This buffer will be used to store the recent ambient light values.
  std::unique_ptr<AmbientLightSampleBuffer> ambient_light_values_;

  base::Optional<AlsReader::AlsInitStatus> als_init_status_;
  base::Optional<bool> brightness_monitor_success_;
  bool model_initialized_ = false;

  base::Optional<bool> power_manager_service_available_;

  Status adapter_status_ = Status::kInitializing;

  // This is set to true whenever a user makes a manual adjustment, and if
  // |params_.user_adjustment_effect| is not |kContinueAuto|. It will be
  // reset to false if |params_.user_adjustment_effect| is |kPauseAuto|.
  bool adapter_disabled_by_user_adjustment_ = false;

  // The thresholds are calculated from |average_ambient_lux_|. They are only
  // updated when brightness should occur (because average ambient value changed
  // sufficiently).
  base::Optional<double> brightening_lux_threshold_;
  base::Optional<double> darkening_lux_threshold_;
  base::Optional<double> immediate_brightening_lux_threshold_;
  base::Optional<double> immediate_darkening_lux_threshold_;

  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> personal_curve_;

  // Average ambient value is only calculated when |CanAdjustBrightness|
  // returns true. This is the average over all values in
  // |ambient_light_values_|. The adapter will notify powerd to change
  // brightness. New thresholds will be calculated from it.
  base::Optional<double> average_ambient_lux_;

  // First time an ALS value was received.
  base::TimeTicks first_als_time_;
  // Latest time an ALS value was received.
  base::TimeTicks latest_als_time_;

  // Last time brightness change occurred.
  base::TimeTicks latest_brightness_change_time_;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_
