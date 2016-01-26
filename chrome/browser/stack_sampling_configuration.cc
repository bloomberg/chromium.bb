// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/stack_sampling_configuration.h"

#include "base/rand_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

namespace {

// The profiler is currently only implemented for Windows x64, and only runs on
// trunk, canary, and dev.
bool IsProfilerSupported() {
#if !defined(_WIN64)
  return false;
#else
  const version_info::Channel channel = chrome::GetChannel();
  return (channel == version_info::Channel::UNKNOWN ||
          channel == version_info::Channel::CANARY ||
          channel == version_info::Channel::DEV);
#endif
}

}  // namespace

StackSamplingConfiguration::StackSamplingConfiguration()
    : configuration_(GenerateConfiguration()) {
}

base::StackSamplingProfiler::SamplingParams
StackSamplingConfiguration::GetSamplingParams() const {
  base::StackSamplingProfiler::SamplingParams params;
  params.bursts = 1;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(30);

  switch (configuration_) {
    case PROFILE_DISABLED:
    case PROFILE_CONTROL:
      params.initial_delay = base::TimeDelta::FromMilliseconds(0);
      params.sampling_interval = base::TimeDelta::FromMilliseconds(0);
      params.samples_per_burst = 0;
      break;

    case PROFILE_NO_SAMPLES:
      params.initial_delay = duration;
      params.sampling_interval = base::TimeDelta::FromMilliseconds(0);
      params.samples_per_burst = 0;
      break;

    case PROFILE_5HZ:
      params.initial_delay = base::TimeDelta::FromMilliseconds(0);
      params.sampling_interval = base::TimeDelta::FromMilliseconds(200);
      params.samples_per_burst = duration / params.sampling_interval;
      break;

    case PROFILE_10HZ:
      params.initial_delay = base::TimeDelta::FromMilliseconds(0);
      params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
      params.samples_per_burst = duration / params.sampling_interval;
      break;

    case PROFILE_100HZ:
      params.initial_delay = base::TimeDelta::FromMilliseconds(0);
      params.sampling_interval = base::TimeDelta::FromMilliseconds(10);
      params.samples_per_burst = duration / params.sampling_interval;
      break;
  }
  return params;
}

bool StackSamplingConfiguration::IsProfilerEnabled() const {
  return (configuration_ != PROFILE_DISABLED &&
          configuration_ != PROFILE_CONTROL);
}

void StackSamplingConfiguration::RegisterSyntheticFieldTrial() const {
  if (!IsProfilerSupported())
    return;

  std::string group;
  switch (configuration_) {
    case PROFILE_DISABLED:
      group = "Disabled";
      break;

    case PROFILE_CONTROL:
      group = "Control";
      break;

    case PROFILE_NO_SAMPLES:
      group = "NoSamples";
      break;

    case PROFILE_5HZ:
      group = "5Hz";
      break;

    case PROFILE_10HZ:
      group = "10Hz";
      break;

    case PROFILE_100HZ:
      group = "100Hz";
      break;
  }

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "SyntheticStackProfilingConfiguration",
      group);
}

// static
StackSamplingConfiguration::ProfileConfiguration
StackSamplingConfiguration::GenerateConfiguration() {
  if (!IsProfilerSupported())
    return PROFILE_DISABLED;

  // Enable the profiler in the intended ultimate production configuration for
  // development/waterfall builds.
  if (chrome::GetChannel() == version_info::Channel::UNKNOWN)
    return PROFILE_10HZ;

  // Enable according to the variations below in canary and dev.
  if (chrome::GetChannel() == version_info::Channel::CANARY ||
      chrome::GetChannel() == version_info::Channel::DEV) {
    struct Variation {
      ProfileConfiguration config;
      int weight;
    };

    // Generate a configuration according to the associated weights.
    const Variation variations[] = {
      { PROFILE_10HZ, 50},
      { PROFILE_CONTROL, 50},
      { PROFILE_DISABLED, 0}
    };

    int total_weight = 0;
    for (const Variation& variation : variations)
      total_weight += variation.weight;
    DCHECK_EQ(100, total_weight);

    int chosen = base::RandInt(0, total_weight - 1);  // Max is inclusive.
    int cumulative_weight = 0;
    for (const Variation& variation : variations) {
      if (chosen >= cumulative_weight &&
          chosen < cumulative_weight + variation.weight) {
        return variation.config;
      }
      cumulative_weight += variation.weight;
    }
    NOTREACHED();
  }

  return PROFILE_DISABLED;
}
