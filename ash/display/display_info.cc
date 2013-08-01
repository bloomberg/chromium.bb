// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <vector>

#include "ash/display/display_info.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/display.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"

#if defined(OS_WIN)
#include "ui/aura/root_window_host.h"
#endif

namespace ash {
namespace internal {

Resolution::Resolution(const gfx::Size& size, bool interlaced)
    : size(size),
      interlaced(interlaced) {
}

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

  // Use larger than max int to catch overflow early.
  static int64 synthesized_display_id = 2200000000LL;

#if defined(OS_WIN)
  gfx::Rect bounds_in_pixel(aura::RootWindowHost::GetNativeScreenSize());
#else
  gfx::Rect bounds_in_pixel(kDefaultHostWindowX, kDefaultHostWindowY,
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
    bounds_in_pixel.SetRect(x, y, width, height);
  }
  if (id == gfx::Display::kInvalidDisplayID)
    id = synthesized_display_id++;
  DisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), has_overscan);
  display_info.set_device_scale_factor(device_scale_factor);
  display_info.set_rotation(rotation);
  display_info.set_ui_scale(ui_scale);
  display_info.SetBounds(bounds_in_pixel);

  // To test the overscan, it creates the default 5% overscan.
  if (has_overscan) {
    int width = bounds_in_pixel.width() / device_scale_factor / 40;
    int height = bounds_in_pixel.height() / device_scale_factor / 40;
    display_info.SetOverscanInsets(gfx::Insets(height, width, height, width));
    display_info.UpdateDisplaySize();
  }

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
      ui_scale_(1.0f),
      native_(false) {
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
      ui_scale_(1.0f),
      native_(false) {
}

DisplayInfo::~DisplayInfo() {
}

void DisplayInfo::Copy(const DisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  DCHECK(!native_info.bounds_in_pixel_.IsEmpty());
  bounds_in_pixel_ = native_info.bounds_in_pixel_;
  size_in_pixel_ = native_info.size_in_pixel_;
  device_scale_factor_ = native_info.device_scale_factor_;
  resolutions_ = native_info.resolutions_;

  // Copy overscan_insets_in_dip_ if it's not empty. This is for test
  // cases which use "/o" annotation which sets the overscan inset
  // to native, and that overscan has to be propagated. This does not
  // happen on the real environment.
  if (!native_info.overscan_insets_in_dip_.empty())
    overscan_insets_in_dip_ = native_info.overscan_insets_in_dip_;

  // Rotation_ and ui_scale_ are given by preference, or unit
  // tests. Don't copy if this native_info came from
  // DisplayChangeObserverX11.
  if (!native_info.native()) {
    rotation_ = native_info.rotation_;
    ui_scale_ = native_info.ui_scale_;
  }
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
  if (!overscan_insets_in_dip_.empty()) {
    gfx::Insets insets_in_pixel =
        overscan_insets_in_dip_.Scale(device_scale_factor_);
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

void DisplayInfo::SetOverscanInsets(const gfx::Insets& insets_in_dip) {
  overscan_insets_in_dip_ = insets_in_dip;
}

gfx::Insets DisplayInfo::GetOverscanInsetsInPixel() const {
  return overscan_insets_in_dip_.Scale(device_scale_factor_);
}

std::string DisplayInfo::ToString() const {
  int rotation_degree = static_cast<int>(rotation_) * 90;
  return base::StringPrintf(
      "DisplayInfo[%lld] native bounds=%s, size=%s, scale=%f, "
      "overscan=%s, rotation=%d, ui-scale=%f",
      static_cast<long long int>(id_),
      bounds_in_pixel_.ToString().c_str(),
      size_in_pixel_.ToString().c_str(),
      device_scale_factor_,
      overscan_insets_in_dip_.ToString().c_str(),
      rotation_degree,
      ui_scale_);
}

std::string DisplayInfo::ToFullString() const {
  std::string resolutions_str;
  std::vector<Resolution>::const_iterator iter = resolutions_.begin();
  for (; iter != resolutions_.end(); ++iter) {
    if (!resolutions_str.empty())
      resolutions_str += ",";
    resolutions_str += iter->size.ToString();
    if (iter->interlaced)
      resolutions_str += "(i)";
  }
  return ToString() + ", resolutions=" + resolutions_str;
}

}  // namespace internal
}  // namespace ash
