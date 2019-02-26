// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer.h"

#include "ash/display/display_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display.h"
#include "ui/display/types/display_snapshot.h"

namespace ash {

DisplayErrorObserver::DisplayErrorObserver() = default;

DisplayErrorObserver::~DisplayErrorObserver() = default;

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    const display::DisplayConfigurator::DisplayStateList& displays,
    display::MultipleDisplayState new_state) {
  bool internal_display_failed = false;
  LOG(ERROR) << "Failed to configure the following display(s):";
  for (auto* display : displays) {
    const int64_t display_id = display->display_id();
    internal_display_failed |=
        display::Display::IsInternalDisplayId(display_id);
    LOG(ERROR) << "- Display with ID = " << display_id << ", and EDID = "
               << base::HexEncode(display->edid().data(),
                                  display->edid().size())
               << ".";
  }

  if (internal_display_failed && displays.size() == 1u) {
    // If the internal display is the only display that failed, don't show this
    // notification to the user, as it's confusing and less helpful.
    // https://crbug.com/775197.
    return;
  }

  base::string16 message =
      (new_state == display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
          ? l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING)
          : ui::SubstituteChromeOSDeviceType(
                IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING);
  ShowDisplayErrorNotification(message, true);
}

}  // namespace ash
