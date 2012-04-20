// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#pragma once

#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"

namespace aura {
class MouseEvent;
class RootWindow;
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {

class WorkspaceManager;

// LayoutManager for top level windows when WorkspaceManager is enabled.
class ASH_EXPORT WorkspaceLayoutManager : public BaseLayoutManager {
 public:
  WorkspaceLayoutManager(aura::RootWindow* root_window,
                         WorkspaceManager* workspace_manager);
  virtual ~WorkspaceLayoutManager();

  // Returns the workspace manager for this container.
  WorkspaceManager* workspace_manager() {
    return workspace_manager_;
  }

  // Overridden from BaseLayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
 protected:
  virtual void ShowStateChanged(aura::Window* window,
                                ui::WindowShowState last_show_state) OVERRIDE;

 private:
  // Owned by WorkspaceController.
  WorkspaceManager* workspace_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
