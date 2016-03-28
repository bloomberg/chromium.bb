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

}  // namespace arc
