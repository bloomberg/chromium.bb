// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/network_service_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace content {
namespace {

#if defined(OS_ANDROID)
// Using 1077 rather than 1024 because 1) it helps ensure that devices with
// exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
// errors and 2) this is the bucket boundary in Memory.Stats.Win.TotalPhys2.
constexpr base::FeatureParam<int> kNetworkServiceOutOfProcessThresholdMb{
    &network::features::kNetworkService, "network_services_oop_threshold_mb",
    1077};
#endif

}  // namespace

bool IsOutOfProcessNetworkService() {
  return base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         !IsInProcessNetworkService();
}

bool IsInProcessNetworkService() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return false;

  if (base::FeatureList::IsEnabled(features::kNetworkServiceInProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return true;
  }

#if defined(OS_ANDROID)
  return base::SysInfo::AmountOfPhysicalMemoryMB() <=
         kNetworkServiceOutOfProcessThresholdMb.Get();
#endif
  return false;
}

}  // namespace content
