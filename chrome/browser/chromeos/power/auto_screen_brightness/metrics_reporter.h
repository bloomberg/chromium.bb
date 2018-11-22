// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_

#include <array>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/metrics/daily_event.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// MetricsReport logs daily user screen brightness adjustments to UMA.
class MetricsReporter : public PowerManagerClient::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UserAdjustment {
    kNoAls = 0,
    kSupportedAls = 1,
    kUnsupportedAls = 2,
    kMaxValue = kUnsupportedAls
  };

  // A histogram recorded in UMA, showing reasons why daily metrics are
  // reported.
  static constexpr char kDailyEventIntervalName[] =
      "AutoScreenBrightness.MetricsDailyEventInterval";

  // Histogram names of daily counts.
  static constexpr char kNoAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.NoAls";
  static constexpr char kSupportedAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.SupportedAls";
  static constexpr char kUnsupportedAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.UnsupportedAls";

  // Registers prefs used by MetricsReporter in |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // RegisterLocalStatePrefs() must be called before instantiating this class.
  MetricsReporter(PowerManagerClient* power_manager_client,
                  PrefService* local_state_pref_service);
  ~MetricsReporter() override;

  // PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& duration) override;

  // Increments number of adjustments with type |user_adjustment|.
  void OnUserBrightnessChangeRequested(UserAdjustment user_adjustment);

  // Calls ReportDailyMetrics directly.
  void ReportDailyMetricsForTesting(metrics::DailyEvent::IntervalType type);

 private:
  class DailyEventObserver;

  // Called by DailyEventObserver whenever a day has elapsed according to
  // |daily_event_|.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  ScopedObserver<PowerManagerClient, PowerManagerClient::Observer>
      power_manager_client_observer_;

  PrefService* pref_service_;  // Not owned.

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer timer_;

  // Daily count for each UserAjustment. Ordered by UserAdjustment values.
  std::array<int, 3> daily_counts_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReporter);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_
