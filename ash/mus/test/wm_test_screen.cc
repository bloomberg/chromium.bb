// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_screen.h"

#include "ui/aura/window.h"
#include "ui/display/display_finder.h"

#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"

namespace ash {
namespace mus {

WmTestScreen::WmTestScreen() {
  display::Screen::SetScreenInstance(this);
}

WmTestScreen::~WmTestScreen() {
  if (display::Screen::GetScreen() == this)
    display::Screen::SetScreenInstance(nullptr);
}

gfx::Point WmTestScreen::GetCursorScreenPoint() {
  // TODO(sky): use the implementation from WindowManagerConnection.
  NOTIMPLEMENTED();
  return gfx::Point();
}

bool WmTestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  if (!window)
    return false;

  return window->IsVisible() &&
         window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

gfx::NativeWindow WmTestScreen::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  NOTREACHED();
  return nullptr;
}

display::Display WmTestScreen::GetPrimaryDisplay() const {
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display WmTestScreen::GetDisplayNearestWindow(
    gfx::NativeView view) const {
  NOTIMPLEMENTED();
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display WmTestScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *display::FindDisplayNearestPoint(display_list_.displays(), point);
}

int WmTestScreen::GetNumDisplays() const {
  return static_cast<int>(display_list_.displays().size());
}

std::vector<display::Display> WmTestScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display WmTestScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* match = display::FindDisplayWithBiggestIntersection(
      display_list_.displays(), match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void WmTestScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WmTestScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace mus
}  // namespace ash
