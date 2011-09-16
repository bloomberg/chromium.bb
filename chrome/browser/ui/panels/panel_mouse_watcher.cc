// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_mouse_watcher.h"

#include "chrome/browser/ui/panels/panel_manager.h"

PanelMouseWatcher::PanelMouseWatcher()
    : bring_up_titlebar_(false), is_testing_mode_enabled_(false) {
}

PanelMouseWatcher::~PanelMouseWatcher() {
}

void PanelMouseWatcher::AddSubscriber(NativePanel* native_panel) {
  if (is_testing_mode_enabled_)
    return;

  if (subscribers_.count(native_panel) == 0) {
    subscribers_.insert(native_panel);
    DCHECK_LE(subscribers_.size(),
              static_cast<size_t>(PanelManager::GetInstance()->num_panels()));

    if (subscribers_.size() == 1)
      Start();
  }
}

void PanelMouseWatcher::RemoveSubscriber(NativePanel* native_panel) {
  if (is_testing_mode_enabled_)
    return;

  DCHECK(subscribers_.count(native_panel) == 1);
  subscribers_.erase(native_panel);

  if (subscribers_.size() == 0)
    Stop();
}

bool PanelMouseWatcher::IsSubscribed(NativePanel* native_panel) const {
  return subscribers_.count(native_panel) != 0;
}

void PanelMouseWatcher::EnableTestingMode() {
  is_testing_mode_enabled_ = true;
  DCHECK(subscribers_.size() == 0);
}

void PanelMouseWatcher::HandleMouseMovement(
    const gfx::Point& mouse_position) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  bool bring_up_titlebar =
      panel_manager->ShouldBringUpTitlebars(
          mouse_position.x(), mouse_position.y());
  if (bring_up_titlebar == bring_up_titlebar_)
    return;
  bring_up_titlebar_ = bring_up_titlebar;
  panel_manager->BringUpOrDownTitlebars(bring_up_titlebar);
}
