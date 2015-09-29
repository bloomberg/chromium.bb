// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_accessor.h"

#include "components/metrics/metrics_service.h"
#include "components/variations/metrics_util.h"

namespace metrics {

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrial(
    MetricsService* metrics_service,
    const std::string& trial_name,
    const std::string& group_name) {
  return RegisterSyntheticFieldTrialWithNameAndGroupHash(
      metrics_service, HashName(trial_name), HashName(group_name));
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    MetricsService* metrics_service,
    uint32_t trial_name_hash,
    const std::string& group_name) {
  return RegisterSyntheticFieldTrialWithNameAndGroupHash(
      metrics_service, trial_name_hash, HashName(group_name));
}

// static
bool MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameAndGroupHash(
    MetricsService* metrics_service,
    uint32_t trial_name_hash,
    uint32_t group_name_hash) {
  if (!metrics_service)
    return false;

  SyntheticTrialGroup trial_group(trial_name_hash, group_name_hash);
  metrics_service->RegisterSyntheticFieldTrial(trial_group);
  return true;
}

}  // namespace metrics
