// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
#define ASH_TEST_DISPLAY_MANAGER_TEST_API_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/types/display_constants.h"

namespace display {
class ManagedDisplayInfo;
}

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
  explicit DisplayManagerTestApi(DisplayManager* display_manager);
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

  // Gets the internal display::ManagedDisplayInfo for a specific display id.
  const display::ManagedDisplayInfo& GetInternalManagedDisplayInfo(
      int64_t display_id);

  // Sets the UI scale for the |display_id|. Returns false if the
  // display_id is not an internal display.
  bool SetDisplayUIScale(int64_t display_id, float scale);

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
  ScopedSetInternalDisplayId(DisplayManager* test_api, int64_t id);
  ~ScopedSetInternalDisplayId();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetInternalDisplayId);
};

// Sets the display mode that matches the |resolution| for |display_id|.
bool SetDisplayResolution(DisplayManager* display_manager,
                          int64_t display_id,
                          const gfx::Size& resolution);

// Creates the dislpay layout from position and offset for the current
// display list. If you simply want to create a new layout that is
// independent of current displays, use DisplayLayoutBuilder or simply
// create a new DisplayLayout and set display id fields (primary, ids
// in placement) manually.
std::unique_ptr<display::DisplayLayout> CreateDisplayLayout(
    DisplayManager* display_manager,
    display::DisplayPlacement::Position position,
    int offset);

// Creates the DisplayIdList from ints.
display::DisplayIdList CreateDisplayIdList2(int64_t id1, int64_t id2);
display::DisplayIdList CreateDisplayIdListN(size_t count, ...);

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
