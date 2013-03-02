// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "ash/display/display_info.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "ui/gfx/display.h"

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

  int x = 0, y = 0, width, height;
  float scale = 1.0f;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, &scale) >= 2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             &scale) >= 4) {
    bounds.SetRect(x, y, width, height);
  }
  if (id == gfx::Display::kInvalidDisplayID)
    id = synthesized_display_id++;
  DisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), false);
  display_info.set_device_scale_factor(scale);
  display_info.SetBounds(bounds);
  DVLOG(1) << "DisplayInfoFromSpec info=" << display_info.ToString()
           << ", spec=" << spec;
  return display_info;
}

DisplayInfo::DisplayInfo()
    : id_(gfx::Display::kInvalidDisplayID),
      has_overscan_(false),
      device_scale_factor_(1.0f),
      overscan_insets_in_dip_(-1, -1, -1, -1),
      has_custom_overscan_insets_(false) {
}

DisplayInfo::DisplayInfo(int64 id,
                         const std::string& name,
                         bool has_overscan)
    : id_(id),
      name_(name),
      has_overscan_(has_overscan),
      device_scale_factor_(1.0f),
      overscan_insets_in_dip_(-1, -1, -1, -1),
      has_custom_overscan_insets_(false) {
}

DisplayInfo::~DisplayInfo() {
}

void DisplayInfo::CopyFromNative(const DisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  DCHECK(!native_info.original_bounds_in_pixel_.IsEmpty());
  original_bounds_in_pixel_ = native_info.original_bounds_in_pixel_;
  bounds_in_pixel_ = native_info.bounds_in_pixel_;
  device_scale_factor_ = native_info.device_scale_factor_;
}

void DisplayInfo::SetBounds(const gfx::Rect& new_original_bounds) {
  original_bounds_in_pixel_ = bounds_in_pixel_ = new_original_bounds;
}

void DisplayInfo::UpdateBounds(const gfx::Rect& new_original_bounds) {
  bool overscan = original_bounds_in_pixel_ != bounds_in_pixel_;
  original_bounds_in_pixel_ = bounds_in_pixel_ = new_original_bounds;
  if (overscan) {
    original_bounds_in_pixel_.Inset(
        overscan_insets_in_dip_.Scale(-device_scale_factor_));
  }
}

void DisplayInfo::UpdateOverscanInfo(bool can_overscan) {
  bounds_in_pixel_ = original_bounds_in_pixel_;
  if (can_overscan) {
    if (has_custom_overscan_insets_) {
      bounds_in_pixel_.Inset(
          overscan_insets_in_dip_.Scale(device_scale_factor_));
    } else if (has_overscan_) {
      // Currently we assume 5% overscan and hope for the best if TV claims it
      // overscan, but doesn't expose how much.
      int width = bounds_in_pixel_.width() / 40;
      int height = bounds_in_pixel_.height() / 40;
      gfx::Insets insets_in_pixel(height, width, height, width);
      overscan_insets_in_dip_ =
          insets_in_pixel.Scale(1.0 / device_scale_factor_);
      bounds_in_pixel_.Inset(
          overscan_insets_in_dip_.Scale(device_scale_factor_));
    }
  }
}

void DisplayInfo::SetOverscanInsets(bool custom,
                                    const gfx::Insets& insets_in_dip) {
  has_custom_overscan_insets_ = custom;
  overscan_insets_in_dip_ = insets_in_dip;
}

std::string DisplayInfo::ToString() const {
  return base::StringPrintf(
      "DisplayInfo[%lld] bounds=%s, original=%s, scale=%f, overscan=%s",
      static_cast<long long int>(id_),
      bounds_in_pixel_.ToString().c_str(),
      original_bounds_in_pixel_.ToString().c_str(),
      device_scale_factor_,
      overscan_insets_in_dip_.ToString().c_str());
}

}  // namespace internal
}  // namespace ash
