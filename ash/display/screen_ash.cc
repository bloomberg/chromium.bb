// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_ash.h"

#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/root_window_finder.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/display_finder.h"

namespace ash {

namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

class ScreenForShutdown : public display::Screen {
 public:
  explicit ScreenForShutdown(ScreenAsh* screen_ash)
      : display_list_(screen_ash->GetAllDisplays()),
        primary_display_(screen_ash->GetPrimaryDisplay()) {
  }

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return NULL;
  }
  int GetNumDisplays() const override { return display_list_.size(); }
  std::vector<display::Display> GetAllDisplays() const override {
    return display_list_;
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeView view) const override {
    return primary_display_;
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return *gfx::FindDisplayNearestPoint(display_list_, point);
  }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    const display::Display* matching =
        gfx::FindDisplayWithBiggestIntersection(display_list_, match_rect);
    // Fallback to the primary display if there is no matching display.
    return matching ? *matching : GetPrimaryDisplay();
  }
  display::Display GetPrimaryDisplay() const override {
    return primary_display_;
  }
  void AddObserver(display::DisplayObserver* observer) override {
    NOTREACHED() << "Observer should not be added during shutdown";
  }
  void RemoveObserver(display::DisplayObserver* observer) override {}

 private:
  const std::vector<display::Display> display_list_;
  const display::Display primary_display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenForShutdown);
};

}  // namespace

ScreenAsh::ScreenAsh() {
}

ScreenAsh::~ScreenAsh() {
}

void ScreenAsh::NotifyMetricsChanged(const display::Display& display,
                                     uint32_t metrics) {
  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayMetricsChanged(display, metrics));
}

void ScreenAsh::NotifyDisplayAdded(const display::Display& display) {
  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayAdded(display));
}

void ScreenAsh::NotifyDisplayRemoved(const display::Display& display) {
  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayRemoved(display));
}

gfx::Point ScreenAsh::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ScreenAsh::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(
             display::Screen::GetScreen()->GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ScreenAsh::GetWindowAtScreenPoint(const gfx::Point& point) {
  aura::Window* root_window =
      wm::WmWindowAura::GetAuraWindow(wm::GetRootWindowAt(point));
  aura::client::ScreenPositionClient* position_client =
      aura::client::GetScreenPositionClient(root_window);

  gfx::Point local_point = point;
  if (position_client)
    position_client->ConvertPointFromScreen(root_window, &local_point);

  return root_window->GetTopWindowContainingPoint(local_point);
}

int ScreenAsh::GetNumDisplays() const {
  return GetDisplayManager()->GetNumDisplays();
}

std::vector<display::Display> ScreenAsh::GetAllDisplays() const {
  return GetDisplayManager()->active_display_list();
}

display::Display ScreenAsh::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  if (!window)
    return GetPrimaryDisplay();
  const aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return GetPrimaryDisplay();
  const RootWindowSettings* rws = GetRootWindowSettings(root_window);
  int64_t id = rws->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != display::Display::kInvalidDisplayID);
  if (id == display::Display::kInvalidDisplayID)
    return GetPrimaryDisplay();

  DisplayManager* display_manager = GetDisplayManager();
  // RootWindow needs Display to determine its device scale factor
  // for non desktop display.
  display::Display mirroring_display =
      display_manager->GetMirroringDisplayById(id);
  if (mirroring_display.is_valid())
    return mirroring_display;
  return display_manager->GetDisplayForId(id);
}

display::Display ScreenAsh::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  const display::Display& display =
      GetDisplayManager()->FindDisplayContainingPoint(point);
  if (display.is_valid())
    return display;
  // Fallback to the display that has the shortest Manhattan distance from
  // the |point|. This is correct in the only areas that matter, namely in the
  // corners between the physical screens.
  return *gfx::FindDisplayNearestPoint(
      GetDisplayManager()->active_display_list(), point);
}

display::Display ScreenAsh::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  if (match_rect.IsEmpty())
    return GetDisplayNearestPoint(match_rect.origin());
  const display::Display* matching = gfx::FindDisplayWithBiggestIntersection(
      GetDisplayManager()->active_display_list(), match_rect);
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

display::Display ScreenAsh::GetPrimaryDisplay() const {
  return GetDisplayManager()->GetDisplayForId(
      WindowTreeHostManager::GetPrimaryDisplayId());
}

void ScreenAsh::AddObserver(display::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenAsh::RemoveObserver(display::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::Screen* ScreenAsh::CloneForShutdown() {
  return new ScreenForShutdown(this);
}

}  // namespace ash
