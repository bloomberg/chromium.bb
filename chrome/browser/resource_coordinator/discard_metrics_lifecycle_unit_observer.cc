// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

DiscardMetricsLifecycleUnitObserver::DiscardMetricsLifecycleUnitObserver() =
    default;
DiscardMetricsLifecycleUnitObserver::~DiscardMetricsLifecycleUnitObserver() =
    default;

void DiscardMetricsLifecycleUnitObserver::OnLifecycleUnitStateChanged(
    LifecycleUnit* lifecycle_unit,
    LifecycleState last_state) {
  if (lifecycle_unit->GetState() == LifecycleState::DISCARDED)
    OnDiscard(lifecycle_unit);
  else if (last_state == LifecycleState::DISCARDED)
    OnReload();
}

void DiscardMetricsLifecycleUnitObserver::OnLifecycleUnitDestroyed(
    LifecycleUnit* lifecycle_unit) {
  // If the browser is not shutting down and the tab is loaded after
  // being discarded, record TabManager.Discarding.ReloadToCloseTime.
  if (g_browser_process && !g_browser_process->IsShuttingDown() &&
      lifecycle_unit->GetState() != LifecycleState::DISCARDED &&
      !reload_time_.is_null()) {
    auto reload_to_close_time = NowTicks() - reload_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "TabManager.Discarding.ReloadToCloseTime", reload_to_close_time,
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  }

  // This is a self-owned object that destroys itself with the LifecycleUnit
  // that it observes.
  lifecycle_unit->RemoveObserver(this);
  delete this;
}

void DiscardMetricsLifecycleUnitObserver::OnDiscard(
    LifecycleUnit* lifecycle_unit) {
  discard_time_ = NowTicks();
  last_focused_time_before_discard_ = lifecycle_unit->GetLastFocusedTime();

  static int discard_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                              ++discard_count, 1, 1000, 50);
}

void DiscardMetricsLifecycleUnitObserver::OnReload() {
  DCHECK(!discard_time_.is_null());
  reload_time_ = NowTicks();

  static int reload_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                              ++reload_count, 1, 1000, 50);
  auto discard_to_reload_time = reload_time_ - discard_time_;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "TabManager.Discarding.DiscardToReloadTime", discard_to_reload_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  auto inactive_to_reload_time =
      reload_time_ - last_focused_time_before_discard_;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "TabManager.Discarding.InactiveToReloadTime", inactive_to_reload_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
}

}  // namespace resource_coordinator
