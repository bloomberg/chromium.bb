// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_STATE_H_
#define ASH_SHELL_STATE_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {

// TODO(jamescook): Move |root_window_for_new_windows_| to Shell and delete
// this class.
class ASH_EXPORT ShellState {
 public:
  ShellState();
  ~ShellState();

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using ScopedRootWindowForNewWindows.
  // NOTE: This returns the root; newly created windows should be added to the
  // appropriate container in the returned window.
  aura::Window* GetRootWindowForNewWindows() const;

  // Updates the root window and notifies observers.
  // NOTE: Prefer ScopedRootWindowForNewWindows.
  void SetRootWindowForNewWindows(aura::Window* root);

 private:
  friend class ScopedRootWindowForNewWindows;

  // Sends a state update to all clients.
  void NotifyAllClients();

  int64_t GetDisplayIdForNewWindows() const;

  // Sets the value and updates clients.
  void SetScopedRootWindowForNewWindows(aura::Window* root);

  aura::Window* root_window_for_new_windows_ = nullptr;

  // See ScopedRootWindowForNewWindows.
  aura::Window* scoped_root_window_for_new_windows_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellState);
};

}  // namespace ash

#endif  // ASH_SHELL_STATE_H_
