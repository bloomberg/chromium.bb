// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

class WindowWatcher::WorkspaceWindowWatcher : public aura::WindowObserver {
 public:
  explicit WorkspaceWindowWatcher(WindowWatcher* watcher) : watcher_(watcher) {
    watcher_->window_->AddObserver(this);
    for (size_t i = 0; i < watcher_->window_->children().size(); ++i)
      watcher_->window_->children()[i]->AddObserver(watcher_);
  }

  virtual ~WorkspaceWindowWatcher() {
    watcher_->window_->RemoveObserver(this);
    for (size_t i = 0; i < watcher_->window_->children().size(); ++i)
      watcher_->window_->children()[i]->RemoveObserver(watcher_);
  }

  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE {
    new_window->AddObserver(watcher_);
  }

  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE {
    DCHECK(window->children().empty());
    window->RemoveObserver(watcher_);
  }

 private:
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowWatcher);
};

WindowWatcher::WindowWatcher()
    : window_(Launcher::ForPrimaryDisplay()->window_container()),
      panel_container_(ash::Shell::GetContainer(
          window_->GetRootWindow(),
          internal::kShellWindowId_PanelContainer)) {
  workspace_window_watcher_.reset(new WorkspaceWindowWatcher(this));
  panel_container_->AddObserver(this);
}

WindowWatcher::~WindowWatcher() {
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
  ash::LauncherModel* model = Shell::GetInstance()->launcher_model();
  ash::LauncherItem item;
  item.type = new_window->type() == aura::client::WINDOW_TYPE_PANEL ?
                                    ash::TYPE_APP_PANEL : ash::TYPE_TABBED;
  id_to_window_[model->next_id()] = new_window;

  SkBitmap icon_bitmap;
  icon_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  icon_bitmap.allocPixels();
  icon_bitmap.eraseARGB(255,
                        image_count == 0 ? 255 : 0,
                        image_count == 1 ? 255 : 0,
                        image_count == 2 ? 255 : 0);
  image_count = (image_count + 1) % 3;
  item.image = gfx::ImageSkia(gfx::ImageSkiaRep(icon_bitmap,
                                                ui::SCALE_FACTOR_100P));

  model->Add(item);
}

void WindowWatcher::OnWillRemoveWindow(aura::Window* window) {
  for (IDToWindow::iterator i = id_to_window_.begin();
       i != id_to_window_.end(); ++i) {
    if (i->second == window) {
      ash::LauncherModel* model = Shell::GetInstance()->launcher_model();
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
