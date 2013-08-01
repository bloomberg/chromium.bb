// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_MANAGER_H_
#define ASH_DISPLAY_DISPLAY_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_info.h"
#include "ash/display/display_layout.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/display.h"

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif

namespace gfx {
class Display;
class Insets;
class Rect;
}

namespace ash {
class AcceleratorControllerTest;
class DisplayController;

namespace test {
class DisplayManagerTestApi;
class SystemGestureEventFilterTest;
}
namespace internal {
class DisplayLayoutStore;

// DisplayManager maintains the current display configurations,
// and notifies observers when configuration changes.
//
// TODO(oshima): Make this non internal.
class ASH_EXPORT DisplayManager
#if defined(OS_CHROMEOS)
    : public chromeos::OutputConfigurator::SoftwareMirroringController
#endif
      {
 public:
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Create or updates the mirror window with |display_info|.
    virtual void CreateOrUpdateMirrorWindow(
        const DisplayInfo& display_info) = 0;

    // Closes the mirror window if exists.
    virtual void CloseMirrorWindow() = 0;

    // Called before and after the display configuration changes.
    virtual void PreDisplayConfigurationChange() = 0;
    virtual void PostDisplayConfigurationChange() = 0;
  };

  // Returns the list of possible UI scales for the display.
  static std::vector<float> GetScalesForDisplay(const DisplayInfo& info);

  // Returns next valid UI scale.
  static float GetNextUIScale(const DisplayInfo& info, bool up);

  // Updates the bounds of the display given by |secondary_display_id|
  // according to |layout|.
  static void UpdateDisplayBoundsForLayoutById(
      const DisplayLayout& layout,
      const gfx::Display& primary_display,
      int64 secondary_display_id);

  DisplayManager();
  virtual ~DisplayManager();

  DisplayLayoutStore* layout_store() {
    return layout_store_.get();
  }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // When set to true, the MonitorManager calls OnDisplayBoundsChanged
  // even if the display's bounds didn't change. Used to swap primary
  // display.
  void set_force_bounds_changed(bool force_bounds_changed) {
    force_bounds_changed_ = force_bounds_changed;
  }

  // Returns the display id of the first display in the outupt list.
  int64 first_display_id() const { return first_display_id_; }

  // Initializes displays using command line flag, or uses
  // defualt if no options are specified.
  void InitFromCommandLine();

  // True if the given |display| is currently connected.
  bool IsActiveDisplay(const gfx::Display& display) const;

  // True if there is an internal display.
  bool HasInternalDisplay() const;

  bool IsInternalDisplayId(int64 id) const;

  // Returns display for given |id|;
  const gfx::Display& GetDisplayForId(int64 id) const;

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  const gfx::Display& FindDisplayContainingPoint(
      const gfx::Point& point_in_screen) const;

  // Sets the work area's |insets| to the display given by |display_id|.
  bool UpdateWorkAreaOfDisplay(int64 display_id, const gfx::Insets& insets);

  // Registers the overscan insets for the display of the specified ID. Note
  // that the insets size should be specified in DIP size. It also triggers the
  // display's bounds change.
  void SetOverscanInsets(int64 display_id, const gfx::Insets& insets_in_dip);

  // Sets the display's rotation.
  void SetDisplayRotation(int64 display_id, gfx::Display::Rotation rotation);

  // Sets the display's ui scale.
  void SetDisplayUIScale(int64 display_id, float ui_scale);

  // Sets the display's resolution.
  void SetDisplayResolution(int64 display_id, const gfx::Size& resolution);

  // Register per display properties. |overscan_insets| is NULL if
  // the display has no custom overscan insets.
  void RegisterDisplayProperty(int64 display_id,
                               gfx::Display::Rotation rotation,
                               float ui_scale,
                               const gfx::Insets* overscan_insets,
                               const gfx::Size& resolution_in_pixels);

  // Returns the display's selected resolution.
  bool GetSelectedResolutionForDisplayId(int64 display_id,
                                         gfx::Size* resolution_out) const;

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

  // Updates current displays using current |display_info_|.
  void UpdateDisplays();

  // Returns the display at |index|. The display at 0 is
  // no longer considered "primary".
  const gfx::Display& GetDisplayAt(size_t index) const;

  const gfx::Display* GetPrimaryDisplayCandidate() const;

  // Returns the logical number of displays. This returns 1
  // when displays are mirrored.
  size_t GetNumDisplays() const;

  // Returns the number of connected displays. This returns 2
  // when displays are mirrored.
  size_t num_connected_displays() const { return num_connected_displays_; }

  // Returns the mirroring status.
  bool IsMirrored() const;
  const gfx::Display& mirrored_display() const { return mirrored_display_; }

  // Retuns the display info associated with |display_id|.
  const DisplayInfo& GetDisplayInfo(int64 display_id) const;

  // Returns the human-readable name for the display |id|.
  std::string GetDisplayNameForId(int64 id);

  // Returns the display id that is capable of UI scaling. On device,
  // this returns internal display's ID if its device scale factor is 2,
  // or invalid ID if such internal display doesn't exist. On linux
  // desktop, this returns the first display ID.
  int64 GetDisplayIdForUIScaling() const;

  // Change the mirror mode.
  void SetMirrorMode(bool mirrored);

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  void AddRemoveDisplay();
  void ToggleDisplayScaleFactor();

  // SoftwareMirroringController override:
#if defined(OS_CHROMEOS)
  virtual void SetSoftwareMirroring(bool enabled) OVERRIDE;
#else
  void SetSoftwareMirroring(bool enabled);
#endif

  // Update the bounds of the display given by |display_id|.
  bool UpdateDisplayBounds(int64 display_id,
                           const gfx::Rect& new_bounds);
private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, ConvertPoint);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest, TestNativeDisplaysChanged);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest,
                           NativeDisplaysChangedAfterPrimaryChange);
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest, AutomaticOverscanInsets);
  friend class ash::AcceleratorControllerTest;
  friend class test::DisplayManagerTestApi;
  friend class test::SystemGestureEventFilterTest;
  friend class DisplayManagerTest;

  typedef std::vector<gfx::Display> DisplayList;

  void set_change_display_upon_host_resize(bool value) {
    change_display_upon_host_resize_ = value;
  }

  gfx::Display* FindDisplayForId(int64 id);

  // Add the mirror display's display info if the software based
  // mirroring is in use.
  void AddMirrorDisplayInfoIfAny(std::vector<DisplayInfo>* display_info_list);

  // Inserts and update the DisplayInfo according to the overscan
  // state. Note that The DisplayInfo stored in the |internal_display_info_|
  // can be different from |new_info| (due to overscan state), so
  // you must use |GetDisplayInfo| to get the correct DisplayInfo for
  // a display.
  void InsertAndUpdateDisplayInfo(const DisplayInfo& new_info);

  // Creates a display object from the DisplayInfo for |display_id|.
  gfx::Display CreateDisplayFromDisplayInfoById(int64 display_id);

  // Updates the bounds of the secondary display in |display_list|
  // using the layout registered for the display pair and set the
  // index of display updated to |updated_index|. Returns true
  // if the secondary display's bounds has been changed from current
  // value, or false otherwise.
  bool UpdateSecondaryDisplayBoundsForLayout(DisplayList* display_list,
                                             size_t* updated_index) const;

  static void UpdateDisplayBoundsForLayout(
      const DisplayLayout& layout,
      const gfx::Display& primary_display,
      gfx::Display* secondary_display);

  Delegate* delegate_;  // not owned.

  scoped_ptr<DisplayLayoutStore> layout_store_;

  int64 first_display_id_;

  gfx::Display mirrored_display_;

  // List of current active dispays.
  DisplayList displays_;

  int num_connected_displays_;

  bool force_bounds_changed_;

  // The mapping from the display ID to its internal data.
  std::map<int64, DisplayInfo> display_info_;

  // Selected resolutions for displays. Key is the displays' ID.
  std::map<int64, gfx::Size> resolutions_;

  // When set to true, the host window's resize event updates
  // the display's size. This is set to true when running on
  // desktop environment (for debugging) so that resizing the host
  // window wil update the display properly. This is set to false
  // on device as well as during the unit tests.
  bool change_display_upon_host_resize_;

  bool software_mirroring_enabled_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_MANAGER_H_
