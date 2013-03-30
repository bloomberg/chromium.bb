// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_

#include "base/basictypes.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class PrefRegistrySimple;

namespace ash {
struct DisplayLayout;
}

namespace gfx {
class Display;
class Insets;
}

namespace chromeos {

// Registers the prefs associated with display settings and stored
// into Local State.
void RegisterDisplayLocalStatePrefs(PrefRegistrySimple* registry);

// Stores the current displays prefereces (both primary display id and
// dispay layout).
void StoreDisplayPrefs();

// Sets the display layout for the current displays and default.
void SetCurrentAndDefaultDisplayLayout(const ash::DisplayLayout& layout);

// Load display preferences from Local Store. |first_run_after_boot| is used
// determine if a certain preference should be applied at boot time or
// restart.
void LoadDisplayPreferences(bool first_run_after_boot);

// Stores the display layout for given display pairs for tests.
void StoreDisplayLayoutPrefForTest(int64 id1,
                                   int64 id2,
                                   const ash::DisplayLayout& layout);

// Stores the given |power_state| for tests.
void StoreDisplayPowerStateForTest(DisplayPowerState power_state);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
