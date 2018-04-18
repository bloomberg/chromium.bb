// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_metrics_type.h"

#include "base/metrics/histogram_macros.h"

namespace ash {

constexpr char kPowerButtonMenuActionHistogram[] =
    "Power.PowerButtonMenuAction";

void RecordMenuActionHistogram(PowerButtonMenuActionType type) {
  UMA_HISTOGRAM_ENUMERATION(kPowerButtonMenuActionHistogram, type,
                            PowerButtonMenuActionType::kPowerMenuActionCount);
}

}  // namespace ash
