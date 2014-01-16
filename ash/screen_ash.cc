// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_ash.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {
internal::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

DisplayController* GetDisplayController() {
  return Shell::GetInstance()->display_controller();
}
}  // namespace

ScreenAsh::ScreenAsh() {
}

ScreenAsh::~ScreenAsh() {
}

// static
gfx::Display ScreenAsh::FindDisplayContainingPoint(const gfx::Point& point) {
  return GetDisplayManager()->FindDisplayContainingPoint(point);
}

// static
gfx::Rect ScreenAsh::GetMaximizedWindowBoundsInParent(aura::Window* window) {
  if (internal::GetRootWindowController(window->GetRootWindow())->shelf())
    return GetDisplayWorkAreaBoundsInParent(window);
  else
    return GetDisplayBoundsInParent(window);
}

// static
gfx::Rect ScreenAsh::GetDisplayBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(
      window->parent(),
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());
}

// static
gfx::Rect ScreenAsh::GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(
      window->parent(),
      Shell::GetScreen()->GetDisplayNearestWindow(window).work_area());
}

// static
gfx::Rect ScreenAsh::ConvertRectToScreen(aura::Window* window,
                                         const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static
gfx::Rect ScreenAsh::ConvertRectFromScreen(aura::Window* window,
                                           const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static
const gfx::Display& ScreenAsh::GetSecondaryDisplay() {
  internal::DisplayManager* display_manager = GetDisplayManager();
  CHECK_EQ(2U, display_manager->GetNumDisplays());
  return display_manager->GetDisplayAt(0).id() ==
      DisplayController::GetPrimaryDisplay().id() ?
      display_manager->GetDisplayAt(1) : display_manager->GetDisplayAt(0);
}

// static
const gfx::Display& ScreenAsh::GetDisplayForId(int64 display_id) {
  return GetDisplayManager()->GetDisplayForId(display_id);
}

void ScreenAsh::NotifyBoundsChanged(const gfx::Display& display) {
  FOR_EACH_OBSERVER(gfx::DisplayObserver, observers_,
                    OnDisplayBoundsChanged(display));
}

void ScreenAsh::NotifyDisplayAdded(const gfx::Display& display) {
  FOR_EACH_OBSERVER(gfx::DisplayObserver, observers_, OnDisplayAdded(display));
}

void ScreenAsh::NotifyDisplayRemoved(const gfx::Display& display) {
  FOR_EACH_OBSERVER(
      gfx::DisplayObserver, observers_, OnDisplayRemoved(display));
}

bool ScreenAsh::IsDIPEnabled() {
  return true;
}

gfx::Point ScreenAsh::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow ScreenAsh::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(Shell::GetScreen()->GetCursorScreenPoint());
}

gfx::NativeWindow ScreenAsh::GetWindowAtScreenPoint(const gfx::Point& point) {
  return wm::GetRootWindowAt(point)->GetTopWindowContainingPoint(point);
}

int ScreenAsh::GetNumDisplays() const {
  return DisplayController::GetNumDisplays();
}

std::vector<gfx::Display> ScreenAsh::GetAllDisplays() const {
  if (!Shell::HasInstance())
    return std::vector<gfx::Display>(1, GetPrimaryDisplay());
  return GetDisplayManager()->displays();
}

gfx::Display ScreenAsh::GetDisplayNearestWindow(gfx::NativeView window) const {
  if (!Shell::HasInstance())
    return GetPrimaryDisplay();
  return GetDisplayController()->GetDisplayNearestWindow(window);
}

gfx::Display ScreenAsh::GetDisplayNearestPoint(const gfx::Point& point) const {
  if (!Shell::HasInstance())
    return GetPrimaryDisplay();
  return GetDisplayController()->GetDisplayNearestPoint(point);
}

gfx::Display ScreenAsh::GetDisplayMatching(const gfx::Rect& match_rect) const {
  if (!Shell::HasInstance())
    return GetPrimaryDisplay();
  return GetDisplayController()->GetDisplayMatching(match_rect);
}

gfx::Display ScreenAsh::GetPrimaryDisplay() const {
  return DisplayController::GetPrimaryDisplay();
}

void ScreenAsh::AddObserver(gfx::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenAsh::RemoveObserver(gfx::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
