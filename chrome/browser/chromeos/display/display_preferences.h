// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_

#include "base/basictypes.h"

class PrefService;
namespace gfx {
class Display;
class Insets;
}

namespace chromeos {

// Registers the prefs associated with display settings and stored
// into Local State.
void RegisterDisplayLocalStatePrefs(PrefService* local_state);

// Sets or updates the display layout data to the specified |display| and
// |pref_service|.
void SetDisplayLayoutPref(const gfx::Display& display,
                          int layout,
                          int offset);

// Stores the specified ID as the primary display ID to Local State.  Clears
// the data if the internal display's ID is specified.
void StorePrimaryDisplayIDPref(int64 display_id);

// Sets or updates the primary display device by its ID, and notifies the update
// to the system.
void SetPrimaryDisplayIDPref(int64 display_id);

// Sets or updates the overscan preference for the specified |display| to Local
// State.
void SetDisplayOverscan(const gfx::Display& display, const gfx::Insets& insets);

// Checks the current display settings in Local State and notifies them to the
// system.
void NotifyDisplayLocalStatePrefChanged();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
