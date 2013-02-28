// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_INFO_H_
#define ASH_DISPLAY_DISPLAY_INFO_H_

#include <string>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Display;
}

namespace ash {
namespace internal {

// DisplayInfo contains metadata for each display. This is used to
// create |gfx::Display| as well as to maintain extra infomation
// to manage displays in ash environment.
// This class is intentionally made copiable.
class ASH_EXPORT DisplayInfo {
 public:
  // Creates a DisplayInfo from string spec. 100+200-1440x800 creates display
  // whose size is 1440x800 at the location (100, 200) in screen's coordinates.
  // The location can be omitted and be just "1440x800", which creates
  // display at the origin of the screen. An empty string creates
  // the display with default size.
  //  The device scale factor can be specified by "*", like "1280x780*2",
  // or will use the value of |gfx::Display::GetForcedDeviceScaleFactor()| if
  // --force-device-scale-factor is specified.
  static DisplayInfo CreateFromSpec(const std::string& spec);

  // Creates a DisplayInfo from string spec using given |id|.
  static DisplayInfo CreateFromSpecWithID(const std::string& spec,
                                          int64 id);

  DisplayInfo();
  DisplayInfo(int64 id, const std::string& name, bool has_overscan);
  ~DisplayInfo();

  int64 id() const { return id_; }

  // The name of the display.
  const std::string& name() const { return name_; }

  // True if the display has overscan.
  bool has_overscan() const { return has_overscan_; }

  // Gets/Sets the device scale factor of the display.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // The original bounds_in_pixel for the display.  This can be different from
  // the |bounds_in_pixel| in case of overscan insets.
  const gfx::Rect original_bounds_in_pixel() const {
    return original_bounds_in_pixel_;
  }

  // The bounds for the display in pixels.
  const gfx::Rect bounds_in_pixel() const { return bounds_in_pixel_; }

  // The overscan insets for the display in DIP. The default value is
  // (-1, -1, -1, -1), which indicates that no overscan should be applied.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  // Copy the display info except for two fields that can be modified by a user
  // (|has_custom_overscan_insets_| and |custom_overscan_insets_in_dip_|).
  void CopyFromNative(const DisplayInfo& native_info);

  // Set the |original_bounds_in_pixel| and |bounds_in_pixel| to
  // given |bounds|.
  void SetBounds(const gfx::Rect& bounds_in_pixel);

  // Sets the |bounds_in_pixel| and updates original bounds based on
  // current overscan configuration.
  void UpdateBounds(const gfx::Rect& bounds_in_pixel);

  // Update the |bounds_in_pixel| according to the current overscan
  // settings.
  // 1) If can_overscan is false, then |bounds_in_pixel| is equal to
  //    |original_bounds_in_pixel|.
  // 2) If this has custom overscan insets
  //    (i.e. |has_custom_overscan_insets_| is true), it simply applies
  //    the existing |overscan_insets_in_dip_|.
  // 3) If this doesn't have custom overscan insets, then this updates
  //    |overscan_insets_in_dip_| to default value (5% of the display size)
  //    and apply the insets.
  void UpdateOverscanInfo(bool can_overscan);

  // Sets/Clears the overscan insets.
  void SetOverscanInsets(bool custom,
                         const gfx::Insets& insets_in_dip);
  void clear_has_custom_overscan_insets() {
    has_custom_overscan_insets_ = false;
  }

  // Returns a string representation of the DisplayInfo;
  std::string ToString() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DisplayManagerTest, AutomaticOverscanInsets);
  // Set the overscan flag. Used for test.
  void set_has_overscan_for_test(bool has_overscan) {
    has_overscan_ = has_overscan;
  }

  int64 id_;
  std::string name_;
  bool has_overscan_;
  float device_scale_factor_;
  gfx::Rect original_bounds_in_pixel_;
  gfx::Rect bounds_in_pixel_;
  gfx::Insets overscan_insets_in_dip_;

  // True if the |overscan_insets_in_dip| is specified by a user. This
  // is used not to override the insets by native insets.
  bool has_custom_overscan_insets_;
};

}  // namespace internal
}  // namespace ash

#endif  //  ASH_DISPLAY_DISPLAY_INFO_H_
