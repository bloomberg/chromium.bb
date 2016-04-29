// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_screen.h"

#include <stdint.h>

#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

namespace chromecast {

namespace {

const int64_t kDisplayId = 1;

const int k720pWidth = 1280;
const int k720pHeight = 720;

// When CastScreen is first initialized, we may not have any display info
// available. These constants hold the initial size that we default to, and
// the size can be updated when we have actual display info at hand with
// UpdateDisplaySize().
const int kInitDisplayWidth = k720pWidth;
const int kInitDisplayHeight = k720pHeight;

}  // namespace

CastScreen::~CastScreen() {
}

void CastScreen::SetDisplayResizeCallback(const DisplayResizeCallback& cb) {
  DCHECK(!cb.is_null());
  display_resize_cb_ = cb;
}

void CastScreen::UpdateDisplaySize(const gfx::Size& size) {
  display_.SetScaleAndBounds(1.0f, gfx::Rect(size));
  if (!display_resize_cb_.is_null())
    display_resize_cb_.Run(Size(size.width(), size.height()));
}

gfx::Point CastScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow CastScreen::GetWindowUnderCursor() {
  NOTIMPLEMENTED();
  return gfx::NativeWindow(nullptr);
}

gfx::NativeWindow CastScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return gfx::NativeWindow(nullptr);
}

int CastScreen::GetNumDisplays() const {
  return 1;
}

std::vector<display::Display> CastScreen::GetAllDisplays() const {
  return std::vector<display::Display>(1, display_);
}

display::Display CastScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

display::Display CastScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

display::Display CastScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

display::Display CastScreen::GetPrimaryDisplay() const {
  return display_;
}

void CastScreen::AddObserver(display::DisplayObserver* observer) {}

void CastScreen::RemoveObserver(display::DisplayObserver* observer) {}

CastScreen::CastScreen() : display_(kDisplayId) {
  display_.SetScaleAndBounds(1.0f,
                             gfx::Rect(kInitDisplayWidth, kInitDisplayHeight));
}

}  // namespace chromecast
