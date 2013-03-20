// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <vector>

#include "ash/display/display_info.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "ui/gfx/display.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"

#if defined(OS_WIN)
#include "ui/aura/root_window_host.h"
#endif

namespace ash {
namespace internal {

// satic
DisplayInfo DisplayInfo::CreateFromSpec(const std::string& spec) {
  return CreateFromSpecWithID(spec, gfx::Display::kInvalidDisplayID);
}

// static
DisplayInfo DisplayInfo::CreateFromSpecWithID(const std::string& spec,
                                              int64 id) {
  // Default bounds for a display.
  const int kDefaultHostWindowX = 200;
  const int kDefaultHostWindowY = 200;
  const int kDefaultHostWindowWidth = 1366;
  const int kDefaultHostWindowHeight = 768;

  static int64 synthesized_display_id = 1000;

#if defined(OS_WIN)
  gfx::Rect bounds(aura::RootWindowHost::GetNativeScreenSize());
#else
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
#endif
  std::string main_spec = spec;

  float ui_scale = 1.0f;
  std::vector<std::string> parts;
  if (Tokenize(main_spec, "@", &parts) == 2) {
    double scale_in_double = 0;
    if (base::StringToDouble(parts[1], &scale_in_double))
      ui_scale = scale_in_double;
    main_spec = parts[0];
  }

  size_t count = Tokenize(main_spec, "/", &parts);
  gfx::Display::Rotation rotation(gfx::Display::ROTATE_0);
  bool has_overscan = false;
  if (count) {
    main_spec = parts[0];
    if (count >= 2) {
      std::string options = parts[1];
      for (size_t i = 0; i < options.size(); ++i) {
        char c = options[i];
        switch (c) {
          case 'o':
            has_overscan = true;
            break;
          case 'r':  // rotate 90 degrees to 'right'.
            rotation = gfx::Display::ROTATE_90;
            break;
          case 'u':  // 180 degrees, 'u'pside-down.
            rotation = gfx::Display::ROTATE_180;
            break;
          case 'l':  // rotate 90 degrees to 'left'.
            rotation = gfx::Display::ROTATE_270;
            break;
        }
      }
    }
  }

  int x = 0, y = 0, width, height;
  float device_scale_factor = 1.0f;
  if (sscanf(main_spec.c_str(), "%dx%d*%f",
             &width, &height, &device_scale_factor) >= 2 ||
      sscanf(main_spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             &device_scale_factor) >= 4) {
    bounds.SetRect(x, y, width, height);
  }
  if (id == gfx::Display::kInvalidDisplayID)
    id = synthesized_display_id++;
  DisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), has_overscan);
  display_info.set_device_scale_factor(device_scale_factor);
  display_info.set_rotation(rotation);
  display_info.set_ui_scale(ui_scale);
  display_info.SetBounds(bounds);
  DVLOG(1) << "DisplayInfoFromSpec info=" << display_info.ToString()
           << ", spec=" << spec;
  return display_info;
}

DisplayInfo::DisplayInfo()
    : id_(gfx::Display::kInvalidDisplayID),
      has_overscan_(false),
      rotation_(gfx::Display::ROTATE_0),
      device_scale_factor_(1.0f),
      overscan_insets_in_dip_(0, 0, 0, 0),
      has_custom_overscan_insets_(false),
      ui_scale_(1.0f) {
}

DisplayInfo::DisplayInfo(int64 id,
                         const std::string& name,
                         bool has_overscan)
    : id_(id),
      name_(name),
      has_overscan_(has_overscan),
      rotation_(gfx::Display::ROTATE_0),
      device_scale_factor_(1.0f),
      overscan_insets_in_dip_(0, 0, 0, 0),
      has_custom_overscan_insets_(false),
      ui_scale_(1.0f) {
}

DisplayInfo::~DisplayInfo() {
}

void DisplayInfo::CopyFromNative(const DisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  DCHECK(!native_info.bounds_in_pixel_.IsEmpty());
  bounds_in_pixel_ = native_info.bounds_in_pixel_;
  size_in_pixel_ = native_info.size_in_pixel_;
  device_scale_factor_ = native_info.device_scale_factor_;
  rotation_ = native_info.rotation_;
  ui_scale_ = native_info.ui_scale_;
  // Don't copy insets as it may be given by preference.  |rotation_|
  // is treated as a native so that it can be specified in
  // |CreateFromSpec|.
}

void DisplayInfo::SetBounds(const gfx::Rect& new_bounds_in_pixel) {
  bounds_in_pixel_ = new_bounds_in_pixel;
  size_in_pixel_ = new_bounds_in_pixel.size();
  UpdateDisplaySize();
}

void DisplayInfo::UpdateDisplaySize() {
  size_in_pixel_ = bounds_in_pixel_.size();
  if (has_custom_overscan_insets_) {
    gfx::Insets insets_in_pixel =
        overscan_insets_in_dip_.Scale(device_scale_factor_);
    size_in_pixel_.Enlarge(-insets_in_pixel.width(), -insets_in_pixel.height());
  } else if (has_overscan_) {
    // Currently we assume 5% overscan and hope for the best if TV claims it
    // overscan, but doesn't expose how much.
    // TODO(oshima): The insets has to be applied after rotation.
    // Fix this.
    int width = bounds_in_pixel_.width() / 40;
    int height = bounds_in_pixel_.height() / 40;
    gfx::Insets insets_in_pixel(height, width, height, width);
    overscan_insets_in_dip_ =
        insets_in_pixel.Scale(1.0 / device_scale_factor_);
    size_in_pixel_.Enlarge(-insets_in_pixel.width(), -insets_in_pixel.height());
  } else {
    overscan_insets_in_dip_.Set(0, 0, 0, 0);
  }

  if (rotation_ == gfx::Display::ROTATE_90 ||
      rotation_ == gfx::Display::ROTATE_270)
    size_in_pixel_.SetSize(size_in_pixel_.height(), size_in_pixel_.width());
  gfx::SizeF size_f(size_in_pixel_);
  size_f.Scale(ui_scale_);
  size_in_pixel_ = gfx::ToFlooredSize(size_f);
}

void DisplayInfo::SetOverscanInsets(bool custom,
                                    const gfx::Insets& insets_in_dip) {
  has_custom_overscan_insets_ = custom;
  overscan_insets_in_dip_ = insets_in_dip;
}

gfx::Insets DisplayInfo::GetOverscanInsetsInPixel() const {
  return overscan_insets_in_dip_.Scale(device_scale_factor_);
}

std::string DisplayInfo::ToString() const {
  int rotation_degree = static_cast<int>(rotation_) * 90;
  return base::StringPrintf(
      "DisplayInfo[%lld] bounds=%s, size=%s, scale=%f, "
      "overscan=%s, rotation=%d, ui-scale=%f",
      static_cast<long long int>(id_),
      bounds_in_pixel_.ToString().c_str(),
      size_in_pixel_.ToString().c_str(),
      device_scale_factor_,
      overscan_insets_in_dip_.ToString().c_str(),
      rotation_degree,
      ui_scale_);
}

}  // namespace internal
}  // namespace ash
