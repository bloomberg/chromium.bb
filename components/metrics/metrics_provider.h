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
  MetricsProvider();
  virtual ~MetricsProvider();

  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog();

  // Called when metrics recording has been enabled.
  virtual void OnRecordingEnabled();

  // Called when metrics recording has been disabled.
  virtual void OnRecordingDisabled();

  // Provides additional metrics into the system profile.
  virtual void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto);

  // Called once at startup to see whether this provider has stability events
  // to share. Default implementation always returns false.
  virtual bool HasStabilityMetrics();

  // Provides additional stability metrics. Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto);

  // Called to indicate that saved stability prefs should be cleared, e.g.
  // because they are from an old version and should not be kept.
  virtual void ClearSavedStabilityMetrics();

  // Provides general metrics that are neither system profile nor stability
  // metrics. May also be used to add histograms when final metrics are
  // collected right before upload.
  virtual void ProvideGeneralMetrics(
      ChromeUserMetricsExtension* uma_proto);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PROVIDER_H_
