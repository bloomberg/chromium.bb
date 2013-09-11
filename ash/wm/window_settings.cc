// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_settings.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/display.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WindowSettings*);

namespace ash {
namespace wm {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(WindowSettings,
                                 kWindowSettingsKey, NULL);

WindowSettings::WindowSettings(aura::Window* window)
    : window_(window),
      tracked_by_workspace_(true),
      window_position_managed_(false),
      bounds_changed_by_user_(false),
      panel_attached_(true),
      continue_drag_after_reparent_(false),
      ignored_by_shelf_(false) {
}

WindowSettings::~WindowSettings() {
}

void WindowSettings::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WindowSettings::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowSettings::SetTrackedByWorkspace(bool tracked_by_workspace) {
  if (tracked_by_workspace_ == tracked_by_workspace)
    return;
  bool old = tracked_by_workspace_;
  tracked_by_workspace_ = tracked_by_workspace;
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnTrackedByWorkspaceChanged(window_, old));
}

WindowSettings* GetWindowSettings(aura::Window* window) {
  WindowSettings* settings = window->GetProperty(kWindowSettingsKey);
  if(!settings) {
    settings = new WindowSettings(window);
    window->SetProperty(kWindowSettingsKey, settings);
  }
  return settings;
}

const WindowSettings* GetWindowSettings(const aura::Window* window) {
  return GetWindowSettings(const_cast<aura::Window*>(window));
}

}  // namespace wm
}  // namespace ash
