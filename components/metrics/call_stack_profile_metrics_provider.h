// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // Receives SampledProfile protobuf instances. May be called on any thread.
  static void ReceiveProfile(base::TimeTicks profile_start_time,
                             SampledProfile profile);

  // Receives serialized SampledProfile protobuf instances. May be called on any
  // thread.  Note that receiving serialized profiles is supported separately so
  // that profiles received in serialized form can be kept in that form until
  // upload. This significantly reduces memory costs. Serialized profile strings
  // may be large, so the caller should use std::move() to provide them to this
  // API rather than copying by value.
  static void ReceiveSerializedProfile(base::TimeTicks profile_start_time,
                                       std::string serialized_sampled_profile);

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // base::Feature for reporting profiles. Provided here for test use.
  static const base::Feature kEnableReporting;

  // Reset the static state to the defaults after startup.
  static void ResetStaticStateForTesting();

 private:
  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
