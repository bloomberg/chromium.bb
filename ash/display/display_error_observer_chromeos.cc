// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer_chromeos.h"

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "ash/display/display_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

DisplayErrorObserver::DisplayErrorObserver() {}

DisplayErrorObserver::~DisplayErrorObserver() {}

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    const display::DisplayConfigurator::DisplayStateList& displays,
    display::MultipleDisplayState new_state) {
  LOG(ERROR) << "Failed to configure the following display(s):";
  for (auto* display : displays) {
    LOG(ERROR) << "- Display with ID = " << display->display_id()
               << ", and EDID = " << base::HexEncode(display->edid().data(),
                                                     display->edid().size())
               << ".";
  }

  base::string16 message =
      (new_state == display::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR)
          ? l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING)
          : ash::SubstituteChromeOSDeviceType(
                IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING);
  ShowDisplayErrorNotification(message, true);
}

}  // namespace ash
