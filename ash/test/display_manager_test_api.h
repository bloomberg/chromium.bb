// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
#define ASH_TEST_DISPLAY_MANAGER_TEST_API_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/display/types/display_constants.h"

namespace gfx {
class Point;
class Size;
}

namespace ui {
namespace test {
class EventGenerator;
}
}

namespace ash {
class DisplayManager;

namespace test {

class DisplayManagerTestApi {
 public:
  // Test if moving a mouse to |point_in_screen| warps it to another
  // display.
  static bool TestIfMouseWarpsAt(ui::test::EventGenerator& event_generator,
                                 const gfx::Point& point_in_screen);

  DisplayManagerTestApi();
  virtual ~DisplayManagerTestApi();

  // Update the display configuration as given in |display_specs|. The format of
  // |display_spec| is a list of comma separated spec for each displays. Please
  // refer to the comment in |ash::DisplayInfo::CreateFromSpec| for the format
  // of the display spec.
  void UpdateDisplay(const std::string& display_specs);

  // Set the 1st display as an internal display and returns the display Id for
  // the internal display.
  int64_t SetFirstDisplayAsInternalDisplay();

  // Don't update the display when the root window's size was changed.
  void DisableChangeDisplayUponHostResize();

  // Sets the available color profiles for |display_id|.
  void SetAvailableColorProfiles(
      int64_t display_id,
      const std::vector<ui::ColorCalibrationProfile>& profiles);

 private:
  friend class ScopedSetInternalDisplayId;
  // Sets the display id for internal display and
  // update the display mode list if necessary.
  void SetInternalDisplayId(int64_t id);

  DisplayManager* display_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerTestApi);
};

class ScopedDisable125DSFForUIScaling {
 public:
  ScopedDisable125DSFForUIScaling();
  ~ScopedDisable125DSFForUIScaling();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedDisable125DSFForUIScaling);
};

class ScopedSetInternalDisplayId {
 public:
  ScopedSetInternalDisplayId(int64_t id);
  ~ScopedSetInternalDisplayId();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetInternalDisplayId);
};

// Sets the display mode that matches the |resolution| for |display_id|.
bool SetDisplayResolution(int64_t display_id, const gfx::Size& resolution);

// Swap the primary display with the secondary.
void SwapPrimaryDisplay();

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
