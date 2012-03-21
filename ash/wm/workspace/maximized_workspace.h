// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MAXIMIZED_WORKSPACE_H_
#define ASH_WM_WORKSPACE_MAXIMIZED_WORKSPACE_H_

#include "ash/wm/workspace/workspace.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

// Workspace implementation that contains a single maximized or fullscreen
// window.
class ASH_EXPORT MaximizedWorkspace : public Workspace {
 public:
  explicit MaximizedWorkspace(WorkspaceManager* manager);
  virtual ~MaximizedWorkspace();

 protected:
  // Workspace overrides:
  virtual bool CanAdd(aura::Window* window) const OVERRIDE;
  virtual void OnWindowAddedAfter(aura::Window* window,
                                  aura::Window* after) OVERRIDE;
  virtual void OnWindowRemoved(aura::Window* window) OVERRIDE;

 private:
  void ResetWindowBounds(aura::Window* window);

  DISALLOW_COPY_AND_ASSIGN(MaximizedWorkspace);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MAXIMIZED_WORKSPACE_H_
