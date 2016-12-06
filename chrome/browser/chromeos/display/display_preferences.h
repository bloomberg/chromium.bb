// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_

#include <stdint.h>
#include <array>

#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display_layout.h"

class PrefRegistrySimple;

namespace gfx {
class Point;
}

namespace chromeos {

// Registers the prefs associated with display settings and stored
// into Local State.
void RegisterDisplayLocalStatePrefs(PrefRegistrySimple* registry);

// Stores the current displays prefereces (both primary display id and
// dispay layout).
void StoreDisplayPrefs();

// If there is an internal display, stores |rotation_lock| along with the
// current rotation of the internal display. Otherwise no data is stored.
void StoreDisplayRotationPrefs(bool rotation_lock);

// Load display preferences from Local Store. |first_run_after_boot| is used
// determine if a certain preference should be applied at boot time or
// restart.
void LoadDisplayPreferences(bool first_run_after_boot);

// Stores the display layout for given display pairs for tests.
void StoreDisplayLayoutPrefForTest(const display::DisplayIdList& list,
                                   const display::DisplayLayout& layout);

// Stores the given |power_state| for tests.
void StoreDisplayPowerStateForTest(DisplayPowerState power_state);

// Parses the marshalled string data stored in local preferences for calibration
// points and populates |point_pair_quad| using the unmarshalled data.
// See TouchCalibrationData in Managed display info.
bool ParseTouchCalibrationStringForTest(
    const std::string& str,
    std::array<std::pair<gfx::Point, gfx::Point>, 4>* point_pair_quad);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
