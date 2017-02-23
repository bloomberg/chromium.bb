// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class SystemProfileProto;

// Stores and loads system information to prefs for stability logs.
class StabilityMetricsProvider : public MetricsProvider {
 public:
  StabilityMetricsProvider(PrefService* local_state);
  ~StabilityMetricsProvider() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void RecordBreakpadRegistration(bool success);
  void RecordBreakpadHasDebugger(bool has_debugger);

  void CheckLastSessionEndCompleted();
  void MarkSessionEndCompleted(bool end_completed);

  void LogCrash();
  void LogStabilityLogDeferred();
  void LogStabilityDataDiscarded();
  void LogLaunch();
  void LogStabilityVersionMismatch();

 private:
  // Increments an Integer pref value specified by |path|.
  void IncrementPrefValue(const char* path);

  // MetricsProvider:
  void ClearSavedStabilityMetrics() override;
  void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) override;

  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(StabilityMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
