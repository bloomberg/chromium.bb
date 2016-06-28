// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

#include "base/metrics/histogram_macros.h"

namespace arc {

void UpdateOptInActionUMA(OptInActionType type) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInAction", static_cast<int>(type),
                            static_cast<int>(OptInActionType::SIZE));
}

void UpdateOptInCancelUMA(OptInCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInCancel", static_cast<int>(reason),
                            static_cast<int>(OptInCancelReason::SIZE));
}

void UpdateEnabledStateUMA(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Arc.State", enabled);
}

void UpdateProvisioningResultUMA(ProvisioningResult result) {
  UMA_HISTOGRAM_ENUMERATION("Arc.Provisioning.Result", static_cast<int>(result),
                            static_cast<int>(ProvisioningResult::SIZE));
}

void UpdateProvisioningTiming(const base::TimeDelta& elapsed_time,
                              bool success,
                              bool managed) {
  std::string histogram_name = "Arc.Provisioning.TimeDelta.";
  histogram_name += success ? "Success." : "Failure.";
  histogram_name += managed ? "Managed" : "Unmanaged";
  // The macro UMA_HISTOGRAM_CUSTOM_TIMES expects a constant string, but since
  // this measurement happens very infrequently, we do not need to use a macro
  // here.
  base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromMinutes(6), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->AddTime(elapsed_time);
}

}  // namespace arc
