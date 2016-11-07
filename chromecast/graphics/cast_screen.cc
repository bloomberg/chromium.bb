// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_screen.h"

#include <stdint.h>

#include "base/command_line.h"
#include "chromecast/public/graphics_properties_shlib.h"
#include "ui/aura/env.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace chromecast {

namespace {

const int64_t kDisplayId = 1;

// Helper to return the screen resolution (device pixels)
// to use.
gfx::Size GetScreenResolution() {
  if (GraphicsPropertiesShlib::IsSupported(
          GraphicsPropertiesShlib::k1080p,
          base::CommandLine::ForCurrentProcess()->argv())) {
    return gfx::Size(1920, 1080);
  } else {
    return gfx::Size(1280, 720);
  }
}

}  // namespace

CastScreen::~CastScreen() {
}

gfx::Point CastScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool CastScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow CastScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return gfx::NativeWindow(nullptr);
}

display::Display CastScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

CastScreen::CastScreen() {
  // Device scale factor computed relative to 720p display
  display::Display display(kDisplayId);
  const gfx::Size size = GetScreenResolution();
  const float device_scale_factor = size.height() / 720.0f;
  display.SetScaleAndBounds(device_scale_factor, gfx::Rect(size));
  ProcessDisplayChanged(display, true /* is_primary */);
}

}  // namespace chromecast
