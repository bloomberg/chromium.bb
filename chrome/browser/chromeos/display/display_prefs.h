// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFS_H_

#include <stdint.h>
#include <array>

#include "base/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace display {
struct MixedMirrorModeParams;
struct TouchCalibrationData;
}

namespace chromeos {

// Manages display preference settings. Settings are stored in the local state
// for the session.
class DisplayPrefs {
 public:
  // Registers the prefs associated with display settings.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // TODO(stevenjb): Move to ash::Shell::display_prefs().
  static DisplayPrefs* Get();

  explicit DisplayPrefs(PrefService* local_state);

  ~DisplayPrefs();

  // Stores all current displays preferences.
  void StoreDisplayPrefs();

  // Loads display preferences from |local_state_|. |first_run_after_boot| is
  // used to determine whether power state preferences should be applied.
  void LoadDisplayPreferences(bool first_run_after_boot);

  // Test helper methods.

  void StoreDisplayRotationPrefsForTest(display::Display::Rotation rotation,
                                        bool rotation_lock);
  void StoreDisplayLayoutPrefForTest(const display::DisplayIdList& list,
                                     const display::DisplayLayout& layout);
  void StoreDisplayPowerStateForTest(DisplayPowerState power_state);
  void LoadTouchAssociationPreferenceForTest();
  void StoreLegacyTouchDataForTest(int64_t display_id,
                                   const display::TouchCalibrationData& data);
  // Parses the marshalled string data stored in local preferences for
  // calibration points and populates |point_pair_quad| using the unmarshalled
  // data. See TouchCalibrationData in Managed display info.
  bool ParseTouchCalibrationStringForTest(
      const std::string& str,
      std::array<std::pair<gfx::Point, gfx::Point>, 4>* point_pair_quad);

  // Stores the given |mixed_params| for tests. Clears stored parameters if
  // |mixed_params| is null.
  void StoreDisplayMixedMirrorModeParamsForTest(
      const base::Optional<display::MixedMirrorModeParams>& mixed_params);

 private:
  PrefService* local_state_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFS_H_
