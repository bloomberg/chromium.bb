// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace metrics {

// ProfilerMetricsProvider is responsible for filling out the |profiler_event|
// section of the UMA proto.
class ProfilerMetricsProvider : public MetricsProvider {
 public:
  ProfilerMetricsProvider();
  virtual ~ProfilerMetricsProvider();

  // MetricsDataProvider:
  virtual void ProvideGeneralMetrics(
      ChromeUserMetricsExtension* uma_proto) OVERRIDE;

  // Records the passed profiled data, which should be a snapshot of the
  // browser's profiled performance during startup for a single process.
  void RecordProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type);

 private:
  // Saved cache of generated Profiler event protos, to be copied into the UMA
  // proto when ProvideGeneralMetrics() is called.
  ProfilerEventProto profiler_event_cache_;

  // True if this instance has recorded profiler data since the last call to
  // ProvideGeneralMetrics().
  bool has_profiler_data_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_PROFILER_METRICS_PROVIDER_H_
