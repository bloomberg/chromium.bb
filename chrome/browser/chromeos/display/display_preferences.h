// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_

#include "base/basictypes.h"

class PrefService;
namespace gfx {
class Display;
}

namespace chromeos {

// Registers the prefs associated with display settings.
void RegisterDisplayPrefs(PrefService* pref_service);

// Sets or updates the display layout data to the specified |display| and
// |pref_service|.
void SetDisplayLayoutPref(PrefService* pref_service,
                          const gfx::Display& display,
                          int layout,
                          int offset);

// Stores the specified ID as the primary display ID to |pref_service|.  Clears
// the data if the internal display's ID is specified.
void StorePrimaryDisplayIDPref(PrefService* pref_service, int64 display_id);

// Sets or updates the primary display device by its ID, and notifies the update
// to the system.
void SetPrimaryDisplayIDPref(PrefService* pref_service, int64 display_id);

// Checks the current display settings values and notifies them to the system.
void NotifyDisplayPrefChanged(PrefService* pref_service);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
