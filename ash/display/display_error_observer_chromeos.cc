// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer_chromeos.h"

#include "ash/display/display_util.h"
#include "base/strings/string_number_conversions.h"
#include "grit/ash_strings.h"

namespace ash {

DisplayErrorObserver::DisplayErrorObserver() {
}

DisplayErrorObserver::~DisplayErrorObserver() {
}

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState new_state) {
  LOG(ERROR) << "Failed to configure the following display(s):";
  for (const auto& display : displays) {
    LOG(ERROR) << "- Display with ID = " << display->display_id()
               << ", and EDID = " << base::HexEncode(display->edid().data(),
                                                     display->edid().size())
               << ".";
  }

  ShowDisplayErrorNotification(
      (new_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR)
          ? IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING
          : IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING);
}

}  // namespace ash
