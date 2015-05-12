// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/ui_setup_android.h"

#include "base/memory/singleton.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/screen.h"

namespace html_viewer {
namespace {

class ScreenMandoline : public gfx::Screen {
 public:
  ScreenMandoline(const gfx::Size& screen_size_in_pixels,
                  float device_pixel_ratio)
      : screen_size_in_pixels_(screen_size_in_pixels),
        device_pixel_ratio_(device_pixel_ratio) {}

  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }

  gfx::NativeWindow GetWindowUnderCursor() override {
    NOTIMPLEMENTED();
    return NULL;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return NULL;
  }

  gfx::Display GetPrimaryDisplay() const override {
    const gfx::Rect bounds_in_pixels(gfx::Point(), screen_size_in_pixels_);
    const gfx::Rect bounds_in_dip(gfx::ToCeiledSize(
        gfx::ScaleSize(bounds_in_pixels.size(), 1.0f / device_pixel_ratio_)));
    gfx::Display display(0, bounds_in_dip);
    display.set_device_scale_factor(device_pixel_ratio_);
    return display;
  }

  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return GetPrimaryDisplay();
  }

  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    return GetPrimaryDisplay();
  }

  int GetNumDisplays() const override { return 1; }

  std::vector<gfx::Display> GetAllDisplays() const override {
    return std::vector<gfx::Display>(1, GetPrimaryDisplay());
  }

  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    return GetPrimaryDisplay();
  }

  void AddObserver(gfx::DisplayObserver* observer) override {
    // no display change on Android.
  }

  void RemoveObserver(gfx::DisplayObserver* observer) override {
    // no display change on Android.
  }

 private:
  const gfx::Size screen_size_in_pixels_;
  const float device_pixel_ratio_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMandoline);
};

}  // namespace

// TODO(sky): this needs to come from system.
class GestureConfigurationMandoline : public ui::GestureConfiguration {
 public:
  GestureConfigurationMandoline() : GestureConfiguration() {
    set_double_tap_enabled(false);
    set_double_tap_timeout_in_ms(semi_long_press_time_in_ms());
    set_gesture_begin_end_types_enabled(true);
    set_min_gesture_bounds_length(default_radius());
    set_min_pinch_update_span_delta(0);
    set_min_scaling_touch_major(default_radius() * 2);
    set_velocity_tracker_strategy(
        ui::VelocityTracker::Strategy::LSQ2_RESTRICTED);
    set_span_slop(max_touch_move_in_pixels_for_click() * 2);
    set_swipe_enabled(true);
    set_two_finger_tap_enabled(true);
  }

  ~GestureConfigurationMandoline() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureConfigurationMandoline);
};

UISetup::UISetup(const gfx::Size& screen_size_in_pixels,
                 float device_pixel_ratio)
    : screen_(new ScreenMandoline(screen_size_in_pixels, device_pixel_ratio)),
      gesture_configuration_(new GestureConfigurationMandoline) {
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                 screen_.get());
  ui::GestureConfiguration::SetInstance(gesture_configuration_.get());
}

UISetup::~UISetup() {
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, nullptr);
  ui::GestureConfiguration::SetInstance(nullptr);
}

}  // namespace html_viewer
