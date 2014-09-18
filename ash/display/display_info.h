// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_INFO_H_
#define ASH_DISPLAY_DISPLAY_INFO_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace ash {

// A struct that represents the display's mode info.
struct ASH_EXPORT DisplayMode {
  DisplayMode();
  DisplayMode(const gfx::Size& size,
              float refresh_rate,
              bool interlaced,
              bool native);

  // Returns the size in DIP which isvisible to the user.
  gfx::Size GetSizeInDIP() const;

  // Returns true if |other| has same size and scale factors.
  bool IsEquivalent(const DisplayMode& other) const;

  gfx::Size size;      // Physical pixel size of the display.
  float refresh_rate;  // Refresh rate of the display, in Hz.
  bool interlaced;     // True if mode is interlaced.
  bool native;         // True if mode is native mode of the display.
  float ui_scale;      // The UI scale factor of the mode.
  float device_scale_factor;  // The device scale factor of the mode.
};

// DisplayInfo contains metadata for each display. This is used to
// create |gfx::Display| as well as to maintain extra infomation
// to manage displays in ash environment.
// This class is intentionally made copiable.
class ASH_EXPORT DisplayInfo {
 public:
  // Creates a DisplayInfo from string spec. 100+200-1440x800 creates display
  // whose size is 1440x800 at the location (100, 200) in host coordinates.
  // The format is
  //
  // [origin-]widthxheight[*device_scale_factor][#resolutions list]
  //     [/<properties>][@ui-scale]
  //
  // where [] are optional:
  // - |origin| is given in x+y- format.
  // - |device_scale_factor| is either 2 or 1 (or empty).
  // - properties can combination of 'o', which adds default overscan insets
  //   (5%), and one rotation property where 'r' is 90 degree clock-wise
  //   (to the 'r'ight) 'u' is 180 degrees ('u'pside-down) and 'l' is
  //   270 degrees (to the 'l'eft).
  // - ui-scale is floating value, e.g. @1.5 or @1.25.
  // - |resolution list| is the list of size that is given in
  //   |width x height [% refresh_rate]| separated by '|'.
  //
  // A couple of examples:
  // "100x100"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 ui scale.
  // "5+5-300x200*2"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 ui scale.
  // "300x200/ol"
  //      300x200 window at 0,0 origin. 1x device scale factor.
  //      with 5% overscan. rotated to left (90 degree counter clockwise).
  //      1.0 ui scale.
  // "10+20-300x200/u@1.5"
  //      300x200 window at 10,20 origin. 1x device scale factor.
  //      no overscan. flipped upside-down (180 degree) and 1.5 ui scale.
  // "200x100#300x200|200x100%59.0|100x100%60"
  //      200x100 window at 0,0 origin, with 3 possible resolutions,
  //      300x200, 200x100 at 59 Hz, and 100x100 at 60 Hz.
  static DisplayInfo CreateFromSpec(const std::string& spec);

  // Creates a DisplayInfo from string spec using given |id|.
  static DisplayInfo CreateFromSpecWithID(const std::string& spec,
                                          int64 id);

  DisplayInfo();
  DisplayInfo(int64 id, const std::string& name, bool has_overscan);
  ~DisplayInfo();

  // When this is set to true on the device whose internal display has
  // 1.25 dsf, Chrome uses 1.0f as a default scale factor, and uses
  // dsf 1.25 when UI scaling is set to 0.8f.
  static void SetUse125DSFForUIScaling(bool enable);

  int64 id() const { return id_; }

  // The name of the display.
  const std::string& name() const { return name_; }

  // True if the display EDID has the overscan flag. This does not create the
  // actual overscan automatically, but used in the message.
  bool has_overscan() const { return has_overscan_; }

  void set_rotation(gfx::Display::Rotation rotation) { rotation_ = rotation; }
  gfx::Display::Rotation rotation() const { return rotation_; }

  void set_touch_support(gfx::Display::TouchSupport support) {
    touch_support_ = support;
  }
  gfx::Display::TouchSupport touch_support() const { return touch_support_; }

  void set_touch_device_id(int id) { touch_device_id_ = id; }
  int touch_device_id() const { return touch_device_id_; }

  // Gets/Sets the device scale factor of the display.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // The native bounds for the display. The size of this can be
  // different from the |size_in_pixel| when overscan insets are set
  // and/or |configured_ui_scale_| is set.
  const gfx::Rect& bounds_in_native() const {
    return bounds_in_native_;
  }

  // The size for the display in pixels.
  const gfx::Size& size_in_pixel() const { return size_in_pixel_; }

  // The overscan insets for the display in DIP.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  // Sets/gets configured ui scale. This can be different from the ui
  // scale actually used when the scale is 2.0 and DSF is 2.0.
  // (the effective ui scale is 1.0 in this case).
  float configured_ui_scale() const { return configured_ui_scale_; }
  void set_configured_ui_scale(float scale) { configured_ui_scale_ = scale; }

  // Returns the ui scale and device scale factor actually used to create
  // display that chrome sees. This can be different from one obtained
  // from dispaly or one specified by a user in following situation.
  // 1) DSF is 2.0f and UI scale is 2.0f. (Returns 1.0f and 1.0f respectiely)
  // 2) A user specified 0.8x on the device that has 1.25 DSF. 1.25 DSF device
  //    uses 1.0f DFS unless 0.8x UI scaling is specified.
  float GetEffectiveDeviceScaleFactor() const;

  // Returns the ui scale used for the device scale factor. This
  // return 1.0f if the ui scale and dsf are both set to 2.0.
  float GetEffectiveUIScale() const;

  // Copy the display info except for fields that can be modified by a
  // user (|rotation_| and |configured_ui_scale_|). |rotation_| and
  // |configured_ui_scale_| are copied when the |another_info| isn't native one.
  void Copy(const DisplayInfo& another_info);

  // Update the |bounds_in_native_| and |size_in_pixel_| using
  // given |bounds_in_native|.
  void SetBounds(const gfx::Rect& bounds_in_native);

  // Update the |bounds_in_native| according to the current overscan
  // and rotation settings.
  void UpdateDisplaySize();

  // Sets/Clears the overscan insets.
  void SetOverscanInsets(const gfx::Insets& insets_in_dip);
  gfx::Insets GetOverscanInsetsInPixel() const;

  void set_native(bool native) { native_ = native; }
  bool native() const { return native_; }

  const std::vector<DisplayMode>& display_modes() const {
    return display_modes_;
  }
  void set_display_modes(std::vector<DisplayMode>& display_modes) {
    display_modes_.swap(display_modes);
  }

  // Returns the native mode size. If a native mode is not present, return an
  // empty size.
  gfx::Size GetNativeModeSize() const;

  ui::ColorCalibrationProfile color_profile() const {
    return color_profile_;
  }

  // Sets the color profile. It will ignore if the specified |profile| is not in
  // |available_color_profiles_|.
  void SetColorProfile(ui::ColorCalibrationProfile profile);

  // Returns true if |profile| is in |available_color_profiles_|.
  bool IsColorProfileAvailable(ui::ColorCalibrationProfile profile) const;

  const std::vector<ui::ColorCalibrationProfile>&
      available_color_profiles() const {
    return available_color_profiles_;
  }

  void set_available_color_profiles(
      const std::vector<ui::ColorCalibrationProfile>& profiles) {
    available_color_profiles_ = profiles;
  }

  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }

  void set_is_aspect_preserving_scaling(bool value) {
    is_aspect_preserving_scaling_ = value;
  }

  // Returns a string representation of the DisplayInfo, excluding display
  // modes.
  std::string ToString() const;

  // Returns a string representation of the DisplayInfo, including display
  // modes.
  std::string ToFullString() const;

 private:
  int64 id_;
  std::string name_;
  bool has_overscan_;
  gfx::Display::Rotation rotation_;
  gfx::Display::TouchSupport touch_support_;

  // If the display is also a touch device, it will have a positive
  // |touch_device_id_|. Otherwise |touch_device_id_| is 0.
  int touch_device_id_;

  // This specifies the device's pixel density. (For example, a
  // display whose DPI is higher than the threshold is considered to have
  // device_scale_factor = 2.0 on Chrome OS).  This is used by the
  // grapics layer to choose and draw appropriate images and scale
  // layers properly.
  float device_scale_factor_;
  gfx::Rect bounds_in_native_;

  // The size of the display in use. The size can be different from the size
  // of |bounds_in_native_| if the display has overscan insets and/or rotation.
  gfx::Size size_in_pixel_;
  gfx::Insets overscan_insets_in_dip_;

  // The pixel scale of the display. This is used to simply expand (or
  // shrink) the desktop over the native display resolution (useful in
  // HighDPI display).  Note that this should not be confused with the
  // device scale factor, which specifies the pixel density of the
  // display. The actuall scale value to be used depends on the device
  // scale factor.  See |GetEffectiveScaleFactor()|.
  float configured_ui_scale_;

  // True if this comes from native platform (DisplayChangeObserver).
  bool native_;

  // True if the display is configured to preserve the aspect ratio. When the
  // display is configured in a non-native mode, only parts of the display will
  // be used such that the aspect ratio is preserved.
  bool is_aspect_preserving_scaling_;

  // The list of modes supported by this display.
  std::vector<DisplayMode> display_modes_;

  // The current profile of the color calibration.
  ui::ColorCalibrationProfile color_profile_;

  // The list of available variations for the color calibration.
  std::vector<ui::ColorCalibrationProfile> available_color_profiles_;
};

}  // namespace ash

#endif  //  ASH_DISPLAY_DISPLAY_INFO_H_
