// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/metrics/metrics_provider.h"

namespace browser_watcher {

// Provides stability data captured by the Chrome Watcher, namely the browser
// process exit codes, as well as exit funnel metrics.
// The exit funnel records a trace of named, timed events in registry per
// process. For reporting, the trace is recorded as a sequence of events
// named Stability.ExitFunnel.<eventname>, associated to the time
// (in milliseconds) from first event in a trace. For a normal process exit,
// the sequence might look like this:
// - Stability.ExitFunnel.Logoff: 0
// - Stability.ExitFunnel.NotifyShutdown: 10
// - Stability.ExitFunnel.EndSession: 20
// - Stability.ExitFunnel.KillProcess: 30
class WatcherMetricsProviderWin : public metrics::MetricsProvider {
 public:
  static const char kBrowserExitCodeHistogramName[];
  static const char kExitFunnelHistogramPrefix[];

  // Initializes the reporter. If |report_exit_funnels| is false, the provider
  // will clear the registry data, but not report it.
  WatcherMetricsProviderWin(const base::char16* registry_path,
                            bool report_exit_funnels);
  ~WatcherMetricsProviderWin();

  // metrics::MetricsProvider implementation.
  virtual void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  base::string16 registry_path_;
  bool report_exit_funnels_;

  DISALLOW_COPY_AND_ASSIGN(WatcherMetricsProviderWin);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_
