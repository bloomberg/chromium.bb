// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/stack_sampling_configuration.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

namespace {

bool IsProfilerEnabledForCurrentChannel() {
  const version_info::Channel channel = chrome::GetChannel();
  return (channel == version_info::Channel::UNKNOWN ||
          channel == version_info::Channel::CANARY ||
          channel == version_info::Channel::DEV);
}

}  // namespace

StackSamplingConfiguration::StackSamplingConfiguration()
    // Disabled pending fixes for deadlock scenarios. https://crbug.com/528129.
    : configuration_(PROFILE_DISABLED) {
}

base::StackSamplingProfiler::SamplingParams
StackSamplingConfiguration::GetSamplingParams() const {
  base::StackSamplingProfiler::SamplingParams params;
  params.bursts = 1;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(30);

  switch (configuration_) {
    case PROFILE_DISABLED:
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
  return IsProfilerEnabledForCurrentChannel() &&
      configuration_ != PROFILE_DISABLED;
}

void StackSamplingConfiguration::RegisterSyntheticFieldTrial() const {
  if (!IsProfilerEnabledForCurrentChannel())
    return;

  std::string group;
  switch (configuration_) {
    case PROFILE_DISABLED:
      group = "Disabled";
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
