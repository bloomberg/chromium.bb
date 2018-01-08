// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_PREFS_H_
#define ASH_DISPLAY_DISPLAY_PREFS_H_

#include <stdint.h>
#include <array>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace display {
struct TouchCalibrationData;
}

namespace ash {

class DisplayPrefsTest;

// Manages display preference settings. Settings are stored in the local state
// for the session.
class ASH_EXPORT DisplayPrefs : public ShellObserver {
 public:
  // Registers the prefs associated with display settings.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  DisplayPrefs();
  ~DisplayPrefs() override;

  // ShellObserver
  void OnLocalStatePrefServiceInitialized(PrefService* pref_service) override;

  // Stores all current displays preferences or queues a request until
  // LoadDisplayPreferences is called..
  void StoreDisplayPrefs();

  // Test helper methods.

  void StoreDisplayRotationPrefsForTest(display::Display::Rotation rotation,
                                        bool rotation_lock);
  void StoreDisplayLayoutPrefForTest(const display::DisplayIdList& list,
                                     const display::DisplayLayout& layout);
  void StoreDisplayPowerStateForTest(chromeos::DisplayPowerState power_state);
  void LoadTouchAssociationPreferenceForTest();
  void StoreLegacyTouchDataForTest(int64_t display_id,
                                   const display::TouchCalibrationData& data);
  // Parses the marshalled string data stored in local preferences for
  // calibration points and populates |point_pair_quad| using the unmarshalled
  // data. See TouchCalibrationData in Managed display info.
  bool ParseTouchCalibrationStringForTest(
      const std::string& str,
      std::array<std::pair<gfx::Point, gfx::Point>, 4>* point_pair_quad);

 protected:
  friend class DisplayPrefsTest;

  // Loads display preferences from |local_state| and sets |local_state_|.
  // |first_run_after_boot| is used to determine whether power state preferences
  // should be applied.
  void LoadDisplayPreferences(bool first_run_after_boot,
                              PrefService* local_state);

 private:
  // Set in LoadDisplayPreferencse. If null, StoreDisplayPrefs just sets
  // store_requested_.
  PrefService* local_state_ = nullptr;
  bool store_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisplayPrefs);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_PREFS_H_
