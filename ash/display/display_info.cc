// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_info.h"

#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(OS_WIN)
#include "ui/aura/window_tree_host.h"
#include "ui/display/win/dpi.h"
#endif

namespace ash {
namespace {

// Use larger than max int to catch overflow early.
const int64_t kSynthesizedDisplayIdStart = 2200000000LL;

int64_t synthesized_display_id = kSynthesizedDisplayIdStart;

const float kDpi96 = 96.0;
bool use_125_dsf_for_ui_scaling = true;

// Check the content of |spec| and fill |bounds| and |device_scale_factor|.
// Returns true when |bounds| is found.
bool GetDisplayBounds(
    const std::string& spec, gfx::Rect* bounds, float* device_scale_factor) {
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  if (sscanf(spec.c_str(), "%dx%d*%f",
             &width, &height, device_scale_factor) >= 2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             device_scale_factor) >= 4) {
    bounds->SetRect(x, y, width, height);
    return true;
  }
  return false;
}

// Display mode list is sorted by:
//  * the area in pixels in ascending order
//  * refresh rate in descending order
struct DisplayModeSorter {
  explicit DisplayModeSorter(bool is_internal) : is_internal(is_internal) {}

  bool operator()(const DisplayMode& a, const DisplayMode& b) {
    gfx::Size size_a_dip = a.GetSizeInDIP(is_internal);
    gfx::Size size_b_dip = b.GetSizeInDIP(is_internal);
    if (size_a_dip.GetArea() == size_b_dip.GetArea())
      return (a.refresh_rate > b.refresh_rate);
    return (size_a_dip.GetArea() < size_b_dip.GetArea());
  }

  bool is_internal;
};

}  // namespace

DisplayMode::DisplayMode()
    : refresh_rate(0.0f),
      interlaced(false),
      native(false),
      ui_scale(1.0f),
      device_scale_factor(1.0f) {}

DisplayMode::DisplayMode(const gfx::Size& size,
                         float refresh_rate,
                         bool interlaced,
                         bool native)
    : size(size),
      refresh_rate(refresh_rate),
      interlaced(interlaced),
      native(native),
      ui_scale(1.0f),
      device_scale_factor(1.0f) {}

gfx::Size DisplayMode::GetSizeInDIP(bool is_internal) const {
  gfx::SizeF size_dip(size);
  size_dip.Scale(ui_scale);
  // DSF=1.25 is special on internal display. The screen is drawn with DSF=1.25
  // but it doesn't affect the screen size computation.
  if (use_125_dsf_for_ui_scaling && is_internal && device_scale_factor == 1.25f)
    return gfx::ToFlooredSize(size_dip);
  size_dip.Scale(1.0f / device_scale_factor);
  return gfx::ToFlooredSize(size_dip);
}

bool DisplayMode::IsEquivalent(const DisplayMode& other) const {
  const float kEpsilon = 0.0001f;
  return size == other.size &&
      std::abs(ui_scale - other.ui_scale) < kEpsilon &&
      std::abs(device_scale_factor - other.device_scale_factor) < kEpsilon;
}

// satic
DisplayInfo DisplayInfo::CreateFromSpec(const std::string& spec) {
  return CreateFromSpecWithID(spec, display::Display::kInvalidDisplayID);
}

// static
DisplayInfo DisplayInfo::CreateFromSpecWithID(const std::string& spec,
                                              int64_t id) {
#if defined(OS_WIN)
  gfx::Rect bounds_in_native(
      gfx::Size(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));
#else
  // Default bounds for a display.
  const int kDefaultHostWindowX = 200;
  const int kDefaultHostWindowY = 200;
  const int kDefaultHostWindowWidth = 1366;
  const int kDefaultHostWindowHeight = 768;
  gfx::Rect bounds_in_native(kDefaultHostWindowX, kDefaultHostWindowY,
                             kDefaultHostWindowWidth, kDefaultHostWindowHeight);
#endif
  std::string main_spec = spec;

  float ui_scale = 1.0f;
  std::vector<std::string> parts = base::SplitString(
      main_spec, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    double scale_in_double = 0;
    if (base::StringToDouble(parts[1], &scale_in_double))
      ui_scale = scale_in_double;
    main_spec = parts[0];
  }

  parts = base::SplitString(main_spec, "/", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  display::Display::Rotation rotation(display::Display::ROTATE_0);
  bool has_overscan = false;
  if (!parts.empty()) {
    main_spec = parts[0];
    if (parts.size() >= 2) {
      std::string options = parts[1];
      for (size_t i = 0; i < options.size(); ++i) {
        char c = options[i];
        switch (c) {
          case 'o':
            has_overscan = true;
            break;
          case 'r':  // rotate 90 degrees to 'right'.
            rotation = display::Display::ROTATE_90;
            break;
          case 'u':  // 180 degrees, 'u'pside-down.
            rotation = display::Display::ROTATE_180;
            break;
          case 'l':  // rotate 90 degrees to 'left'.
            rotation = display::Display::ROTATE_270;
            break;
        }
      }
    }
  }

  float device_scale_factor = 1.0f;
  if (!GetDisplayBounds(main_spec, &bounds_in_native, &device_scale_factor)) {
#if defined(OS_WIN)
    device_scale_factor = display::win::GetDPIScale();
#endif
  }

  std::vector<DisplayMode> display_modes;
  parts = base::SplitString(main_spec, "#", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    size_t native_mode = 0;
    int largest_area = -1;
    float highest_refresh_rate = -1.0f;
    main_spec = parts[0];
    std::string resolution_list = parts[1];
    parts = base::SplitString(resolution_list, "|", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY);
    for (size_t i = 0; i < parts.size(); ++i) {
      DisplayMode mode;
      gfx::Rect mode_bounds;
      std::vector<std::string> resolution = base::SplitString(
          parts[i], "%", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (GetDisplayBounds(
              resolution[0], &mode_bounds, &mode.device_scale_factor)) {
        mode.size = mode_bounds.size();
        if (resolution.size() > 1)
          sscanf(resolution[1].c_str(), "%f", &mode.refresh_rate);
        if (mode.size.GetArea() >= largest_area &&
            mode.refresh_rate > highest_refresh_rate) {
          // Use mode with largest area and highest refresh rate as native.
          largest_area = mode.size.GetArea();
          highest_refresh_rate = mode.refresh_rate;
          native_mode = i;
        }
        display_modes.push_back(mode);
      }
    }
    display_modes[native_mode].native = true;
  }

  if (id == display::Display::kInvalidDisplayID)
    id = synthesized_display_id++;
  DisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), has_overscan);
  display_info.set_device_scale_factor(device_scale_factor);
  display_info.SetRotation(rotation, display::Display::ROTATION_SOURCE_ACTIVE);
  display_info.set_configured_ui_scale(ui_scale);
  display_info.SetBounds(bounds_in_native);
  display_info.SetDisplayModes(display_modes);

  // To test the overscan, it creates the default 5% overscan.
  if (has_overscan) {
    int width = bounds_in_native.width() / device_scale_factor / 40;
    int height = bounds_in_native.height() / device_scale_factor / 40;
    display_info.SetOverscanInsets(gfx::Insets(height, width, height, width));
    display_info.UpdateDisplaySize();
  }

  DVLOG(1) << "DisplayInfoFromSpec info=" << display_info.ToString()
           << ", spec=" << spec;
  return display_info;
}

// static
void DisplayInfo::SetUse125DSFForUIScalingForTest(bool enable) {
  use_125_dsf_for_ui_scaling = enable;
}

DisplayInfo::DisplayInfo()
    : id_(display::Display::kInvalidDisplayID),
      has_overscan_(false),
      active_rotation_source_(display::Display::ROTATION_SOURCE_UNKNOWN),
      touch_support_(display::Display::TOUCH_SUPPORT_UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      overscan_insets_in_dip_(0, 0, 0, 0),
      configured_ui_scale_(1.0f),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false),
      color_profile_(ui::COLOR_PROFILE_STANDARD) {}

DisplayInfo::DisplayInfo(int64_t id, const std::string& name, bool has_overscan)
    : id_(id),
      name_(name),
      has_overscan_(has_overscan),
      active_rotation_source_(display::Display::ROTATION_SOURCE_UNKNOWN),
      touch_support_(display::Display::TOUCH_SUPPORT_UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      overscan_insets_in_dip_(0, 0, 0, 0),
      configured_ui_scale_(1.0f),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false),
      color_profile_(ui::COLOR_PROFILE_STANDARD) {}

DisplayInfo::DisplayInfo(const DisplayInfo& other) = default;

DisplayInfo::~DisplayInfo() {
}

void DisplayInfo::SetRotation(display::Display::Rotation rotation,
                              display::Display::RotationSource source) {
  rotations_[source] = rotation;
  rotations_[display::Display::ROTATION_SOURCE_ACTIVE] = rotation;
  active_rotation_source_ = source;
}

display::Display::Rotation DisplayInfo::GetActiveRotation() const {
  return GetRotation(display::Display::ROTATION_SOURCE_ACTIVE);
}

display::Display::Rotation DisplayInfo::GetRotation(
    display::Display::RotationSource source) const {
  if (rotations_.find(source) == rotations_.end())
    return display::Display::ROTATE_0;
  return rotations_.at(source);
}

void DisplayInfo::Copy(const DisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  active_rotation_source_ = native_info.active_rotation_source_;
  touch_support_ = native_info.touch_support_;
  input_devices_ = native_info.input_devices_;
  device_scale_factor_ = native_info.device_scale_factor_;
  DCHECK(!native_info.bounds_in_native_.IsEmpty());
  bounds_in_native_ = native_info.bounds_in_native_;
  device_dpi_ = native_info.device_dpi_;
  size_in_pixel_ = native_info.size_in_pixel_;
  is_aspect_preserving_scaling_ = native_info.is_aspect_preserving_scaling_;
  display_modes_ = native_info.display_modes_;
  available_color_profiles_ = native_info.available_color_profiles_;
  maximum_cursor_size_ = native_info.maximum_cursor_size_;

  // Rotation, ui_scale, color_profile and overscan are given by preference,
  // or unit tests. Don't copy if this native_info came from
  // DisplayChangeObserver.
  if (!native_info.native()) {
    // Update the overscan_insets_in_dip_ either if the inset should be
    // cleared, or has non empty insts.
    if (native_info.clear_overscan_insets())
      overscan_insets_in_dip_.Set(0, 0, 0, 0);
    else if (!native_info.overscan_insets_in_dip_.IsEmpty())
      overscan_insets_in_dip_ = native_info.overscan_insets_in_dip_;

    rotations_ = native_info.rotations_;
    configured_ui_scale_ = native_info.configured_ui_scale_;
    color_profile_ = native_info.color_profile();
  }
}

void DisplayInfo::SetBounds(const gfx::Rect& new_bounds_in_native) {
  bounds_in_native_ = new_bounds_in_native;
  size_in_pixel_ = new_bounds_in_native.size();
  UpdateDisplaySize();
}

float DisplayInfo::GetEffectiveDeviceScaleFactor() const {
  if (Use125DSFForUIScaling() && device_scale_factor_ == 1.25f)
    return (configured_ui_scale_ == 0.8f) ? 1.25f : 1.0f;
  if (device_scale_factor_ == configured_ui_scale_)
    return 1.0f;
  return device_scale_factor_;
}

float DisplayInfo::GetEffectiveUIScale() const {
  if (Use125DSFForUIScaling() && device_scale_factor_ == 1.25f)
    return (configured_ui_scale_ == 0.8f) ? 1.0f : configured_ui_scale_;
  if (device_scale_factor_ == configured_ui_scale_)
    return 1.0f;
  return configured_ui_scale_;
}

void DisplayInfo::UpdateDisplaySize() {
  size_in_pixel_ = bounds_in_native_.size();
  if (!overscan_insets_in_dip_.IsEmpty()) {
    gfx::Insets insets_in_pixel =
        overscan_insets_in_dip_.Scale(device_scale_factor_);
    size_in_pixel_.Enlarge(-insets_in_pixel.width(), -insets_in_pixel.height());
  } else {
    overscan_insets_in_dip_.Set(0, 0, 0, 0);
  }

  if (GetActiveRotation() == display::Display::ROTATE_90 ||
      GetActiveRotation() == display::Display::ROTATE_270) {
    size_in_pixel_.SetSize(size_in_pixel_.height(), size_in_pixel_.width());
  }
  gfx::SizeF size_f(size_in_pixel_);
  size_f.Scale(GetEffectiveUIScale());
  size_in_pixel_ = gfx::ToFlooredSize(size_f);
}

void DisplayInfo::SetOverscanInsets(const gfx::Insets& insets_in_dip) {
  overscan_insets_in_dip_ = insets_in_dip;
}

gfx::Insets DisplayInfo::GetOverscanInsetsInPixel() const {
  return overscan_insets_in_dip_.Scale(device_scale_factor_);
}

void DisplayInfo::SetDisplayModes(
    const std::vector<DisplayMode>& display_modes) {
  display_modes_ = display_modes;
  std::sort(display_modes_.begin(), display_modes_.end(),
            DisplayModeSorter(display::Display::IsInternalDisplayId(id_)));
}

gfx::Size DisplayInfo::GetNativeModeSize() const {
  for (size_t i = 0; i < display_modes_.size(); ++i) {
    if (display_modes_[i].native)
      return display_modes_[i].size;
  }

  return gfx::Size();
}

std::string DisplayInfo::ToString() const {
  int rotation_degree = static_cast<int>(GetActiveRotation()) * 90;
  std::string devices_str;

  for (size_t i = 0; i < input_devices_.size(); ++i) {
    devices_str += base::IntToString(input_devices_[i]);
    if (i != input_devices_.size() - 1)
      devices_str += ", ";
  }

  std::string result = base::StringPrintf(
      "DisplayInfo[%lld] native bounds=%s, size=%s, scale=%f, "
      "overscan=%s, rotation=%d, ui-scale=%f, touchscreen=%s, "
      "input_devices=[%s]",
      static_cast<long long int>(id_), bounds_in_native_.ToString().c_str(),
      size_in_pixel_.ToString().c_str(), device_scale_factor_,
      overscan_insets_in_dip_.ToString().c_str(), rotation_degree,
      configured_ui_scale_,
      touch_support_ == display::Display::TOUCH_SUPPORT_AVAILABLE
          ? "yes"
          : touch_support_ == display::Display::TOUCH_SUPPORT_UNAVAILABLE
                ? "no"
                : "unknown",
      devices_str.c_str());

  return result;
}

std::string DisplayInfo::ToFullString() const {
  std::string display_modes_str;
  std::vector<DisplayMode>::const_iterator iter = display_modes_.begin();
  for (; iter != display_modes_.end(); ++iter) {
    if (!display_modes_str.empty())
      display_modes_str += ",";
    base::StringAppendF(&display_modes_str,
                        "(%dx%d@%f%c%s)",
                        iter->size.width(),
                        iter->size.height(),
                        iter->refresh_rate,
                        iter->interlaced ? 'I' : 'P',
                        iter->native ? "(N)" : "");
  }
  return ToString() + ", display_modes==" + display_modes_str;
}

void DisplayInfo::SetColorProfile(ui::ColorCalibrationProfile profile) {
  if (IsColorProfileAvailable(profile))
    color_profile_ = profile;
}

bool DisplayInfo::IsColorProfileAvailable(
    ui::ColorCalibrationProfile profile) const {
  return std::find(available_color_profiles_.begin(),
                   available_color_profiles_.end(),
                   profile) != available_color_profiles_.end();
}

bool DisplayInfo::Use125DSFForUIScaling() const {
  return use_125_dsf_for_ui_scaling &&
         display::Display::IsInternalDisplayId(id_);
}

void DisplayInfo::AddInputDevice(int id) {
  input_devices_.push_back(id);
}

void DisplayInfo::ClearInputDevices() {
  input_devices_.clear();
}

void ResetDisplayIdForTest() {
  synthesized_display_id = kSynthesizedDisplayIdStart;
}

}  // namespace ash
