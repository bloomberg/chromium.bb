// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_watcher.h"

#include <utility>

#include "ash/display/window_tree_host_manager.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell/window_watcher_shelf_item_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"

namespace ash {
namespace shell {

class WindowWatcher::WorkspaceWindowWatcher : public aura::WindowObserver {
 public:
  explicit WorkspaceWindowWatcher(WindowWatcher* watcher) : watcher_(watcher) {
  }

  ~WorkspaceWindowWatcher() override {}

  void OnWindowAdded(aura::Window* new_window) override {
    new_window->AddObserver(watcher_);
  }

  void OnWillRemoveWindow(aura::Window* window) override {
    DCHECK(window->children().empty());
    window->RemoveObserver(watcher_);
  }

  void RootWindowAdded(aura::Window* root) {
    aura::Window* panel_container =
        ash::Shell::GetContainer(root, kShellWindowId_PanelContainer);
    panel_container->AddObserver(watcher_);

    aura::Window* container =
        Shelf::ForWindow(root)->shelf_widget()->window_container();
    container->AddObserver(this);
    for (size_t i = 0; i < container->children().size(); ++i)
      container->children()[i]->AddObserver(watcher_);
  }

  void RootWindowRemoved(aura::Window* root) {
    aura::Window* panel_container =
        ash::Shell::GetContainer(root, kShellWindowId_PanelContainer);
    panel_container->RemoveObserver(watcher_);

    aura::Window* container =
        Shelf::ForWindow(root)->shelf_widget()->window_container();
    container->RemoveObserver(this);
    for (size_t i = 0; i < container->children().size(); ++i)
      container->children()[i]->RemoveObserver(watcher_);
  }

 private:
  WindowWatcher* watcher_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowWatcher);
};

WindowWatcher::WindowWatcher() {
  workspace_window_watcher_.reset(new WorkspaceWindowWatcher(this));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++ iter) {
    workspace_window_watcher_->RootWindowAdded(*iter);
  }
}

WindowWatcher::~WindowWatcher() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++ iter) {
    workspace_window_watcher_->RootWindowRemoved(*iter);
  }
}

aura::Window* WindowWatcher::GetWindowByID(ash::ShelfID id) {
  IDToWindow::const_iterator i = id_to_window_.find(id);
  return i != id_to_window_.end() ? i->second : NULL;
}

// aura::WindowObserver overrides:
void WindowWatcher::OnWindowAdded(aura::Window* new_window) {
  if (!wm::IsWindowUserPositionable(new_window))
    return;

  static int image_count = 0;
  ShelfModel* model = Shell::GetInstance()->shelf_model();
  ShelfItem item;
  item.type = new_window->type() == ui::wm::WINDOW_TYPE_PANEL
                  ? ash::TYPE_APP_PANEL
                  : ash::TYPE_PLATFORM_APP;
  ash::ShelfID id = model->next_id();
  id_to_window_[id] = new_window;

  SkBitmap icon_bitmap;
  icon_bitmap.allocN32Pixels(16, 16);
  icon_bitmap.eraseARGB(255,
                        image_count == 0 ? 255 : 0,
                        image_count == 1 ? 255 : 0,
                        image_count == 2 ? 255 : 0);
  image_count = (image_count + 1) % 3;
  item.image = gfx::ImageSkia(gfx::ImageSkiaRep(icon_bitmap, 1.0f));

  model->Add(item);

  ShelfItemDelegateManager* manager =
      Shell::GetInstance()->shelf_item_delegate_manager();
  std::unique_ptr<ShelfItemDelegate> delegate(
      new WindowWatcherShelfItemDelegate(id, this));
  manager->SetShelfItemDelegate(id, std::move(delegate));
  SetShelfIDForWindow(id, new_window);
}

void WindowWatcher::OnWillRemoveWindow(aura::Window* window) {
  for (IDToWindow::iterator i = id_to_window_.begin();
       i != id_to_window_.end(); ++i) {
    if (i->second == window) {
      ShelfModel* model = Shell::GetInstance()->shelf_model();
      int index = model->ItemIndexByID(i->first);
      DCHECK_NE(-1, index);
      model->RemoveItemAt(index);
      id_to_window_.erase(i);
      break;
    }
  }
}

void WindowWatcher::OnDisplayAdded(const display::Display& new_display) {
  aura::Window* root = Shell::GetInstance()
                           ->window_tree_host_manager()
                           ->GetRootWindowForDisplayId(new_display.id());
  workspace_window_watcher_->RootWindowAdded(root);
}

void WindowWatcher::OnDisplayRemoved(const display::Display& old_display) {
  // All windows in the display has already been removed, so no need to
  // remove observers.
}

void WindowWatcher::OnDisplayMetricsChanged(const display::Display&, uint32_t) {
}

}  // namespace shell
}  // namespace ash
