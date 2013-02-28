// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_MANAGER_H_
#define ASH_DISPLAY_DISPLAY_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"

namespace gfx {
class Display;
class Insets;
class Rect;
}

namespace ash {
class AcceleratorControllerTest;
namespace test {
class DisplayManagerTestApi;
class SystemGestureEventFilterTest;
}
namespace internal {

// DisplayManager maintains the current display configurations,
// and notifies observers when configuration changes.
// This is exported for unittest.
//
// TODO(oshima): gfx::Screen needs to return translated coordinates
// if the root window is translated. crbug.com/119268.
class ASH_EXPORT DisplayManager : public aura::RootWindowObserver {
 public:
  DisplayManager();
  virtual ~DisplayManager();

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  static void CycleDisplay();
  static void ToggleDisplayScale();

  // When set to true, the MonitorManager calls OnDisplayBoundsChanged
  // even if the display's bounds didn't change. Used to swap primary
  // display.
  void set_force_bounds_changed(bool force_bounds_changed) {
    force_bounds_changed_ = force_bounds_changed;
  }

  // True if the given |display| is currently connected.
  bool IsActiveDisplay(const gfx::Display& display) const;

  // True if there is an internal display.
  bool HasInternalDisplay() const;

  bool IsInternalDisplayId(int64 id) const;

  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  // Returns display for given |id|;
  const gfx::Display& GetDisplayForId(int64 id) const;

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  const gfx::Display& FindDisplayContainingPoint(
      const gfx::Point& point_in_screen) const;

  // Registers the overscan insets for the display of the specified ID. Note
  // that the insets size should be specified in DIP size. It also triggers the
  // display's bounds change.
  void SetOverscanInsets(int64 display_id, const gfx::Insets& insets_in_dip);

  // Returns the current overscan insets for the specified |display_id|.
  // Returns an empty insets (0, 0, 0, 0) if no insets are specified for
  // the display.
  gfx::Insets GetOverscanInsets(int64 display_id) const;

  // Called when display configuration has changed. The new display
  // configurations is passed as a vector of Display object, which
  // contains each display's new infomration.
  void OnNativeDisplaysChanged(const std::vector<gfx::Display>& displays);

  // Updates the internal display data and notifies observers about the changes.
  void UpdateDisplays(const std::vector<gfx::Display>& displays);

  // Create a root window for given |display|.
  aura::RootWindow* CreateRootWindowForDisplay(const gfx::Display& display);

  // Obsoleted: Do not use in new code.
  // Returns the display at |index|. The display at 0 is
  // no longer considered "primary".
  gfx::Display* GetDisplayAt(size_t index);

  size_t GetNumDisplays() const;

  // Returns the display object nearest given |window|.
  const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const;

  // Returns the display object nearest given |point|.
  const gfx::Display& GetDisplayNearestWindow(
      const aura::Window* window) const;

  // Returns the display that most closely intersects |match_rect|.
  const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect)const;

  // Returns the human-readable name for the display specified by |display|.
  std::string GetDisplayNameFor(const gfx::Display& display);

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& new_size) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, ConvertPoint);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest, TestNativeDisplaysChanged);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest,
                           NativeDisplaysChangedAfterPrimaryChange);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest, AutomaticOverscanInsets);
  friend class ash::AcceleratorControllerTest;
  friend class test::DisplayManagerTestApi;
  friend class DisplayManagerTest;
  friend class test::SystemGestureEventFilterTest;

  typedef std::vector<gfx::Display> DisplayList;

  // Metadata for each display.
  struct DisplayInfo {
    DisplayInfo();

    // The cached name of the display.
    std::string name;

    // The original bounds_in_pixel for the display.  This can be different from
    // the current one in case of overscan insets.
    gfx::Rect original_bounds_in_pixel;

    // The overscan insets for the display.
    gfx::Insets overscan_insets_in_dip;

    // True if we detect that the display has overscan area. False if the
    // display doesn't have it, or failed to detect it.
    bool has_overscan;

    // True if the |overscan_insets_in_dip| is specified. This is set because
    // the user may specify an empty inset intentionally.
    bool has_custom_overscan_insets;
  };

  void Init();
  void CycleDisplayImpl();
  void ScaleDisplayImpl();

  gfx::Display& FindDisplayForRootWindow(const aura::RootWindow* root);
  gfx::Display& FindDisplayForId(int64 id);

  // Refer to |CreateDisplayFromSpec| API for the format of |spec|.
  void AddDisplayFromSpec(const std::string& spec);

  // Set the 1st display as an internal display and returns the display Id for
  // the internal display.
  int64 SetFirstDisplayAsInternalDisplayForTest();

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void EnsurePointerInDisplays();

  // Updates |display_info_| by calling platform-dependent functions.
  void RefreshDisplayInfo();

  // Update the display's id in the |display_list| to match the ones
  // stored in this display manager's |displays_|. This is used to
  // emulate display change behavior during the test byn creating the
  // display list with the same display ids but with different bounds
  void SetDisplayIdsForTest(DisplayList* display_list) const;

  // Forcibly specify 'has_overscan' flag of the DisplayInfo for specified |id|.
  void SetHasOverscanFlagForTest(int64 id, bool has_overscan);

  DisplayList displays_;

  // An internal display cache used when the internal display is disconnectd.
  scoped_ptr<gfx::Display> internal_display_;

  bool force_bounds_changed_;

  // The mapping from the display ID to its internal data.
  std::map<int64, DisplayInfo> display_info_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

// Creates a display from string spec. 100+200-1440x800 creates display
// whose size is 1440x800 at the location (100, 200) in screen's coordinates.
// The location can be omitted and be just "1440x800", which creates
// display at the origin of the screen. An empty string creates
// the display with default size.
//  The device scale factor can be specified by "*", like "1280x780*2",
// or will use the value of |gfx::Display::GetForcedDeviceScaleFactor()| if
// --force-device-scale-factor is specified.
ASH_EXPORT gfx::Display CreateDisplayFromSpec(const std::string& str);

extern const aura::WindowProperty<int64>* const kDisplayIdKey;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_MANAGER_H_
