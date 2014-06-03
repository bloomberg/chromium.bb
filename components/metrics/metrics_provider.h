// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_METRICS_PROVIDER_H_

#include "base/basictypes.h"

namespace metrics {

class ChromeUserMetricsExtension;
class SystemProfileProto;
class SystemProfileProto_Stability;

// MetricsProvider is an interface allowing different parts of the UMA protos to
// be filled out by different classes.
class MetricsProvider {
 public:
  MetricsProvider() {}
  virtual ~MetricsProvider() {}

  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog() {}

  // Called when metrics recording has been enabled.
  virtual void OnRecordingEnabled() {}

  // Called when metrics recording has been disabled.
  virtual void OnRecordingDisabled() {}

  // Provides additional metrics into the system profile.
  virtual void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) {}

  // Provides additional stability metrics. Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) {}

  // Provides general metrics that are neither system profile nor stability
  // metrics.
  virtual void ProvideGeneralMetrics(
      ChromeUserMetricsExtension* uma_proto) {}

  // TODO(asvitkine): Remove this method. http://crbug.com/379148
  virtual void RecordCurrentState() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PROVIDER_H_
