// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/demo_screen.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect_conversions.h"

namespace mojo {
namespace examples {

// static
DemoScreen* DemoScreen::Create() {
  return new DemoScreen(gfx::Rect(0, 0, 800, 600));
}

DemoScreen::~DemoScreen() {
}

bool DemoScreen::IsDIPEnabled() {
  NOTIMPLEMENTED();
  return true;
}

gfx::Point DemoScreen::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::NativeWindow DemoScreen::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow DemoScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return NULL;
}

int DemoScreen::GetNumDisplays() const {
  return 1;
}

std::vector<gfx::Display> DemoScreen::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, display_);
}

gfx::Display DemoScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display DemoScreen::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

gfx::Display DemoScreen::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display DemoScreen::GetPrimaryDisplay() const {
  return display_;
}

void DemoScreen::AddObserver(gfx::DisplayObserver* observer) {
}

void DemoScreen::RemoveObserver(gfx::DisplayObserver* observer) {
}

DemoScreen::DemoScreen(const gfx::Rect& screen_bounds) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace examples
}  // namespace mojo
