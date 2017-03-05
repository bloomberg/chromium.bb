// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/devicetype_utils.h"

#include "ash/strings/grit/ash_strings.h"
#include "chromeos/system/devicetype.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

base::string16 SubstituteChromeOSDeviceType(int resource_id) {
  return l10n_util::GetStringFUTF16(resource_id, GetChromeOSDeviceName());
}

base::string16 GetChromeOSDeviceName() {
  return l10n_util::GetStringUTF16(GetChromeOSDeviceTypeResourceId());
}

int GetChromeOSDeviceTypeResourceId() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
      return IDS_ASH_CHROMEBASE;
    case chromeos::DeviceType::kChromebook:
      return IDS_ASH_CHROMEBOOK;
    case chromeos::DeviceType::kChromebox:
      return IDS_ASH_CHROMEBOX;
    case chromeos::DeviceType::kChromebit:
      return IDS_ASH_CHROMEBIT;
    case chromeos::DeviceType::kUnknown:
    default:
      return IDS_ASH_CHROMEDEVICE;
  }
}

}  // namespace ash
