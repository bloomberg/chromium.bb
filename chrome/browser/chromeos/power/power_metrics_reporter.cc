// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_metrics_reporter.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal =
    base::TimeDelta::FromSeconds(60);

}  // namespace

// This shim class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to PowerMetricsReporter.
class PowerMetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(PowerMetricsReporter* reporter)
      : reporter_(reporter) {}
  ~DailyEventObserver() override = default;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  PowerMetricsReporter* reporter_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DailyEventObserver);
};

const char PowerMetricsReporter::kDailyEventIntervalName[] =
    "Power.MetricsDailyEventInterval";
const char PowerMetricsReporter::kIdleScreenDimCountName[] =
    "Power.IdleScreenDimCountDaily";
const char PowerMetricsReporter::kIdleScreenOffCountName[] =
    "Power.IdleScreenOffCountDaily";
const char PowerMetricsReporter::kIdleSuspendCountName[] =
    "Power.IdleSuspendCountDaily";

// static
void PowerMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(registry, prefs::kPowerMetricsDailySample);
  registry->RegisterIntegerPref(prefs::kPowerMetricsIdleScreenDimCount, 0);
  registry->RegisterIntegerPref(prefs::kPowerMetricsIdleScreenOffCount, 0);
  registry->RegisterIntegerPref(prefs::kPowerMetricsIdleSuspendCount, 0);
}

PowerMetricsReporter::PowerMetricsReporter(
    PowerManagerClient* power_manager_client,
    PrefService* local_state_pref_service)
    : power_manager_client_(power_manager_client),
      pref_service_(local_state_pref_service),
      daily_event_(
          std::make_unique<metrics::DailyEvent>(pref_service_,
                                                prefs::kPowerMetricsDailySample,
                                                kDailyEventIntervalName)),
      idle_screen_dim_count_(
          pref_service_->GetInteger(prefs::kPowerMetricsIdleScreenDimCount)),
      idle_screen_off_count_(
          pref_service_->GetInteger(prefs::kPowerMetricsIdleScreenOffCount)),
      idle_suspend_count_(
          pref_service_->GetInteger(prefs::kPowerMetricsIdleSuspendCount)) {
  power_manager_client_->AddObserver(this);

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

PowerMetricsReporter::~PowerMetricsReporter() {
  power_manager_client_->RemoveObserver(this);
}

void PowerMetricsReporter::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  if (state.dimmed() && !old_screen_idle_state_.dimmed())
    SetIdleScreenDimCount(idle_screen_dim_count_ + 1);
  if (state.off() && !old_screen_idle_state_.off())
    SetIdleScreenOffCount(idle_screen_off_count_ + 1);

  old_screen_idle_state_ = state;
}

void PowerMetricsReporter::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (reason == power_manager::SuspendImminent_Reason_IDLE)
    SetIdleSuspendCount(idle_suspend_count_ + 1);
}

void PowerMetricsReporter::SuspendDone(const base::TimeDelta& duration) {
  daily_event_->CheckInterval();
}

void PowerMetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    UMA_HISTOGRAM_COUNTS_100(kIdleScreenDimCountName, idle_screen_dim_count_);
    UMA_HISTOGRAM_COUNTS_100(kIdleScreenOffCountName, idle_screen_off_count_);
    UMA_HISTOGRAM_COUNTS_100(kIdleSuspendCountName, idle_suspend_count_);
  }

  SetIdleScreenDimCount(0);
  SetIdleScreenOffCount(0);
  SetIdleSuspendCount(0);
}

void PowerMetricsReporter::SetIdleScreenDimCount(int count) {
  idle_screen_dim_count_ = count;
  pref_service_->SetInteger(prefs::kPowerMetricsIdleScreenDimCount, count);
}

void PowerMetricsReporter::SetIdleScreenOffCount(int count) {
  idle_screen_off_count_ = count;
  pref_service_->SetInteger(prefs::kPowerMetricsIdleScreenOffCount, count);
}

void PowerMetricsReporter::SetIdleSuspendCount(int count) {
  idle_suspend_count_ = count;
  pref_service_->SetInteger(prefs::kPowerMetricsIdleSuspendCount, count);
}

}  // namespace chromeos
