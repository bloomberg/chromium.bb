// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_WATCHER_H_
#define ASH_SHELL_WINDOW_WATCHER_H_
#pragma once

#include <map>

#include "ash/launcher/launcher_types.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ash {
namespace shell {

// WindowWatcher is responsible for listening for newly created windows and
// creating items on the Launcher for them.
class WindowWatcher : public aura::WindowObserver {
 public:
  WindowWatcher();
  virtual ~WindowWatcher();

  aura::Window* GetWindowByID(ash::LauncherID id);
  ash::LauncherID GetIDByWindow(aura::Window* window) const;

  // aura::WindowObserver overrides:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;

 private:
  typedef std::map<ash::LauncherID, aura::Window*> IDToWindow;

  // Window watching for newly created windows to be added to.
  aura::Window* window_;

  aura::Window* panel_container_;

  // Maps from window to the id we gave it.
  IDToWindow id_to_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowWatcher);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_WATCHER_H_
