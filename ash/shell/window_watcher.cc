// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

WindowWatcher::WindowWatcher()
    : window_(ash::Shell::GetInstance()->launcher()->window_container()),
      panel_container_(ash::Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          internal::kShellWindowId_PanelContainer)) {
  window_->AddObserver(this);
  panel_container_->AddObserver(this);
}

WindowWatcher::~WindowWatcher() {
  window_->RemoveObserver(this);
  panel_container_->RemoveObserver(this);
}

aura::Window* WindowWatcher::GetWindowByID(ash::LauncherID id) {
  IDToWindow::const_iterator i = id_to_window_.find(id);
  return i != id_to_window_.end() ? i->second : NULL;
}

ash::LauncherID WindowWatcher::GetIDByWindow(aura::Window* window) const {
  for (IDToWindow::const_iterator i = id_to_window_.begin();
       i != id_to_window_.end(); ++i) {
    if (i->second == window)
      return i->first;
  }
  return 0;  // TODO: add a constant for this.
}

// aura::WindowObserver overrides:
void WindowWatcher::OnWindowAdded(aura::Window* new_window) {
  if (new_window->type() != aura::client::WINDOW_TYPE_NORMAL &&
      new_window->type() != aura::client::WINDOW_TYPE_PANEL)
    return;

  static int image_count = 0;
  ash::LauncherModel* model = ash::Shell::GetInstance()->launcher()->model();
  ash::LauncherItem item;
  item.type = ash::TYPE_TABBED;
  id_to_window_[model->next_id()] = new_window;
  item.image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  item.image.allocPixels();
  item.image.eraseARGB(255,
                       image_count == 0 ? 255 : 0,
                       image_count == 1 ? 255 : 0,
                       image_count == 2 ? 255 : 0);
  image_count = (image_count + 1) % 3;
  model->Add(item);
}

void WindowWatcher::OnWillRemoveWindow(aura::Window* window) {
  for (IDToWindow::iterator i = id_to_window_.begin();
       i != id_to_window_.end(); ++i) {
    if (i->second == window) {
      ash::LauncherModel* model =
          ash::Shell::GetInstance()->launcher()->model();
      int index = model->ItemIndexByID(i->first);
      DCHECK_NE(-1, index);
      model->RemoveItemAt(index);
      id_to_window_.erase(i);
      break;
    }
  }
}

}  // namespace shell
}  // namespace ash
