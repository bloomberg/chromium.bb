// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_MANAGER_H_
#define ASH_DISPLAY_DISPLAY_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_info.h"
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
  static void ToggleDisplayScaleFactor();

  // When set to true, the MonitorManager calls OnDisplayBoundsChanged
  // even if the display's bounds didn't change. Used to swap primary
  // display.
  void set_force_bounds_changed(bool force_bounds_changed) {
    force_bounds_changed_ = force_bounds_changed;
  }

  // Returns the display id of the first display in the outupt list.
  int64 first_display_id() const { return first_display_id_; }

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

  // Clears the overscan insets
  void ClearCustomOverscanInsets(int64 display_id);

  // Sets the display's rotation.
  void SetDisplayRotation(int64 display_id, gfx::Display::Rotation rotation);

  // Sets the display's ui scale.
  void SetDisplayUIScale(int64 display_id, float ui_scale);

  // Tells if display rotation/ui scaling features are enabled.
  bool IsDisplayRotationEnabled() const;
  bool IsDisplayUIScalingEnabled() const;

  // Returns the current overscan insets for the specified |display_id|.
  // Returns an empty insets (0, 0, 0, 0) if no insets are specified for
  // the display.
  gfx::Insets GetOverscanInsets(int64 display_id) const;

  // Called when display configuration has changed. The new display
  // configurations is passed as a vector of Display object, which
  // contains each display's new infomration.
  void OnNativeDisplaysChanged(
      const std::vector<DisplayInfo>& display_info_list);

  // Updates the internal display data and notifies observers about the changes.
  void UpdateDisplays(const std::vector<DisplayInfo>& display_info_list);

  // Obsoleted: Do not use in new code.
  // Returns the display at |index|. The display at 0 is
  // no longer considered "primary".
  gfx::Display* GetDisplayAt(size_t index);

  const gfx::Display* GetPrimaryDisplayCandidate() const;

  // Returns the logical number of displays. This returns 1
  // when displays are mirrored.
  size_t GetNumDisplays() const;

  // Returns the number of connected displays. This returns 2
  // when displays are mirrored.
  size_t num_connected_displays() const { return num_connected_displays_; }

  // Returns the mirroring status.
  bool IsMirrored() const;
  int64 mirrored_display_id() const { return mirrored_display_id_; }

  // Returns the display object nearest given |window|.
  const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const;

  // Returns the display object nearest given |point|.
  const gfx::Display& GetDisplayNearestWindow(
      const aura::Window* window) const;

  // Returns the display that most closely intersects |match_rect|.
  const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect)const;

  // Retuns the display info associated with |display|.
  const DisplayInfo& GetDisplayInfo(const gfx::Display& display) const;

  // Returns the human-readable name for the display |id|.
  std::string GetDisplayNameForId(int64 id);

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

  void set_change_display_upon_host_resize(bool value) {
    change_display_upon_host_resize_ = value;
  }

  void Init();
  void CycleDisplayImpl();
  void ScaleDisplayImpl();

  gfx::Display& FindDisplayForRootWindow(const aura::RootWindow* root);
  gfx::Display& FindDisplayForId(int64 id);

  // Refer to |CreateDisplayFromSpec| API for the format of |spec|.
  void AddDisplayFromSpec(const std::string& spec);

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void EnsurePointerInDisplays();

  // Inserts and update the DisplayInfo according to the overscan
  // state. Note that The DisplayInfo stored in the |internal_display_info_|
  // can be different from |new_info| (due to overscan state), so
  // you must use |GetDisplayInfo| to get the correct DisplayInfo for
  // a display.
  void InsertAndUpdateDisplayInfo(const DisplayInfo& new_info);

  // Creates a display object from the DisplayInfo for |display_id|.
  gfx::Display CreateDisplayFromDisplayInfoById(int64 display_id);

  int64 first_display_id_;

  int64 mirrored_display_id_;

  // List of current active dispays.
  DisplayList displays_;

  int num_connected_displays_;

  // An internal display info cache used when the internal display is
  // disconnectd.
  scoped_ptr<DisplayInfo> internal_display_info_;

  bool force_bounds_changed_;

  // The mapping from the display ID to its internal data.
  std::map<int64, DisplayInfo> display_info_;

  // When set to true, the host window's resize event updates
  // the display's size. This is set to true when running on
  // desktop environment (for debugging) so that resizing the host
  // window wil update the display properly. This is set to false
  // on device as well as during the unit tests.
  bool change_display_upon_host_resize_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

extern const aura::WindowProperty<int64>* const kDisplayIdKey;

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_MANAGER_H_
