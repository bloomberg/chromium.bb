// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_

#include <string>

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
#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config_loader.h"
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
                public ModelConfigLoader::Observer,
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

    // The log of average ambient value has to go up (resp. down) by
    // |brightening_log_lux_threshold| (resp. |darkening_log_lux_threshold|)
    // from the current value before brightness could be changed.
    double brightening_log_lux_threshold = 1.0;
    double darkening_log_lux_threshold = 1.0;

    ModelCurve model_curve = ModelCurve::kLatest;

    // Average ambient value is calculated over the past
    // |auto_brightness_als_horizon|. This is only used for brightness update,
    // which can be different from the horizon used in model training.
    base::TimeDelta auto_brightness_als_horizon =
        base::TimeDelta::FromSeconds(5);

    UserAdjustmentEffect user_adjustment_effect =
        UserAdjustmentEffect::kDisableAuto;

    std::string metrics_key;
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
    // Deprecated.
    kImmediateBrightneningThresholdExceeded = 1,
    // Deprecated.
    kImmediateDarkeningThresholdExceeded = 2,
    kBrightneningThresholdExceeded = 3,
    kDarkeningThresholdExceeded = 4,
    kMaxValue = kDarkeningThresholdExceeded
  };

  Adapter(Profile* profile,
          AlsReader* als_reader,
          BrightnessMonitor* brightness_monitor,
          Modeller* modeller,
          ModelConfigLoader* model_config_loader,
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

  // ModelConfigLoader::Observer overrides:
  void OnModelConfigLoaded(base::Optional<ModelConfig> model_config) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  void SetTickClockForTesting(const base::TickClock* test_tick_clock);

  Status GetStatusForTesting() const;

  // Only returns true if Adapter status is success and it's not disabled by
  // user adjustment.
  bool IsAppliedForTesting() const;
  base::Optional<MonotoneCubicSpline> GetGlobalCurveForTesting() const;
  base::Optional<MonotoneCubicSpline> GetPersonalCurveForTesting() const;

  // Returns the actual log average over |ambient_light_values_|.
  base::Optional<double> GetAverageAmbientForTesting(base::TimeTicks now);
  double GetBrighteningThresholdForTesting() const;
  double GetDarkeningThresholdForTesting() const;

 private:
  // Called by |OnModelConfigLoaded|. It will initialize all params used by
  // the modeller from |model_config| and also other experiment flags. If any
  // param is invalid, it will disable the adapter.
  void InitParams(const ModelConfig& model_config);

  // Called when powerd becomes available.
  void OnPowerManagerServiceAvailable(bool service_is_ready);

  // Called to update |adapter_status_| when there's some status change from
  // AlsReader, BrightnessMonitor, Modeller, power manager and after
  // |InitParams|.
  void UpdateStatus();

  // Returns a BrightnessChangeCause if the adapter can change the brightness.
  // This is generally the case when the brightness hasn't been manually
  // set, we've received enough initial ambient light readings, and
  // the ambient light has changed beyond thresholds.
  // Returns nullopt if it shouldn't change the brightness.
  base::Optional<BrightnessChangeCause> CanAdjustBrightness(
      double current_average_ambient) const;

  // Called when ambient light changes. It only changes screen brightness if
  // |CanAdjustBrightness| returns true and a required curve is set up:
  // if the required curve is personal but no personal curve is available, then
  // brightness won't be changed.
  // It will call |UpdateBrightnessChangeThresholds| if brightness is actually
  // changed.
  void MaybeAdjustBrightness(base::TimeTicks now);

  // This is only called when brightness is changed.
  void UpdateBrightnessChangeThresholds();

  // Calculates brightness from given |ambient_log_lux| based on either
  // |global_curve_| or |personal_curve_| (as specified by the experiment
  // params). Returns nullopt if a personal curve should be used but it's not
  // available.
  base::Optional<double> GetBrightnessBasedOnAmbientLogLux(
      double ambient_log_lux) const;

  Profile* const profile_;

  ScopedObserver<AlsReader, AlsReader::Observer> als_reader_observer_;
  ScopedObserver<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observer_;
  ScopedObserver<Modeller, Modeller::Observer> modeller_observer_;

  ScopedObserver<ModelConfigLoader, ModelConfigLoader::Observer>
      model_config_loader_observer_;

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

  // |model_config_exists_| will remain nullopt until |OnModelConfigLoaded| is
  // called. Its value will then be set to true if the input model config exists
  // (not nullopt), else its value will be false.
  base::Optional<bool> model_config_exists_;

  bool model_initialized_ = false;

  base::Optional<bool> power_manager_service_available_;

  Status adapter_status_ = Status::kInitializing;

  // This is set to true whenever a user makes a manual adjustment, and if
  // |params_.user_adjustment_effect| is not |kContinueAuto|. It will be
  // reset to false if |params_.user_adjustment_effect| is |kPauseAuto|.
  bool adapter_disabled_by_user_adjustment_ = false;

  // The thresholds are calculated from the |log_average_ambient_lux_|.
  // They are only updated when brightness should occur (because the log of
  // average ambient value changed sufficiently).
  base::Optional<double> brightening_threshold_;
  base::Optional<double> darkening_threshold_;

  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> personal_curve_;

  // Average ambient value is only calculated when |CanAdjustBrightness|
  // returns true. This is the log of average over all values in
  // |ambient_light_values_|. The adapter will notify powerd to change
  // brightness. New thresholds will be calculated from it.
  base::Optional<double> log_average_ambient_lux_;

  // Last time brightness change occurred.
  base::TimeTicks latest_brightness_change_time_;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_ADAPTER_H_
