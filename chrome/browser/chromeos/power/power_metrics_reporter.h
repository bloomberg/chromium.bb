// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_POWER_METRICS_REPORTER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/metrics/daily_event.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// PowerMetricsReporter reports power-management-related metrics.
// Prefs are used to retain metrics across Chrome restarts and system reboots.
class PowerMetricsReporter : public PowerManagerClient::Observer {
 public:
  // Histogram names.
  static const char kDailyEventIntervalName[];
  static const char kIdleScreenDimCountName[];
  static const char kIdleScreenOffCountName[];
  static const char kIdleSuspendCountName[];

  // Registers prefs used by PowerMetricsReporter in |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // RegisterLocalStatePrefs() must be called before instantiating this class.
  PowerMetricsReporter(PowerManagerClient* power_manager_client,
                       PrefService* local_state_pref_service);
  ~PowerMetricsReporter() override;

  // PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& duration) override;

 private:
  friend class PowerMetricsReporterTest;

  class DailyEventObserver;

  // Called by DailyEventObserver whenever a day has elapsed according to
  // |daily_event_|.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  // Updates |*_count_| members and the corresponding prefs.
  void SetIdleScreenDimCount(int count);
  void SetIdleScreenOffCount(int count);
  void SetIdleSuspendCount(int count);

  PowerManagerClient* power_manager_client_;  // Not owned.
  PrefService* pref_service_;                 // Not owned.

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer timer_;

  // Last-received screen-idle state from powerd.
  power_manager::ScreenIdleState old_screen_idle_state_;

  // Current counts of various inactivity-triggered power management events.
  int idle_screen_dim_count_ = 0;
  int idle_screen_off_count_ = 0;
  int idle_suspend_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PowerMetricsReporter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_POWER_METRICS_REPORTER_H_
