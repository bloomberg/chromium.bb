// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_WATCHER_H_
#define ASH_SHELL_WINDOW_WATCHER_H_

#include <map>

#include "ash/shelf/shelf_item_types.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display_observer.h"

namespace aura {
class Window;
}

namespace ash {
namespace shell {

// TODO(sky): fix this class, its a bit broke with workspace2.

// WindowWatcher is responsible for listening for newly created windows and
// creating items on the Shelf for them.
class WindowWatcher : public aura::WindowObserver,
                      public gfx::DisplayObserver {
 public:
  WindowWatcher();
  ~WindowWatcher() override;

  aura::Window* GetWindowByID(ash::ShelfID id);

  // aura::WindowObserver overrides:
  void OnWindowAdded(aura::Window* new_window) override;
  void OnWillRemoveWindow(aura::Window* window) override;

  // gfx::DisplayObserver overrides:
  void OnDisplayAdded(const gfx::Display& new_display) override;
  void OnDisplayRemoved(const gfx::Display& old_display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override;

 private:
  class WorkspaceWindowWatcher;

  typedef std::map<ash::ShelfID, aura::Window*> IDToWindow;

  // Maps from window to the id we gave it.
  IDToWindow id_to_window_;

  scoped_ptr<WorkspaceWindowWatcher> workspace_window_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatcher);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_WATCHER_H_
