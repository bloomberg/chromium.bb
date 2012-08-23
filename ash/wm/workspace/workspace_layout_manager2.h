// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER2_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER2_H_

#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"

namespace aura {
class RootWindow;
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {

class Workspace2;
class WorkspaceManager2;

// LayoutManager used on the window created for a Workspace.
class ASH_EXPORT WorkspaceLayoutManager2 : public BaseLayoutManager {
 public:
 public:
  WorkspaceLayoutManager2(aura::RootWindow* root_window, Workspace2* workspace);
  virtual ~WorkspaceLayoutManager2();

  // Overridden from BaseWorkspaceLayoutManager:
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overriden from WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 protected:
  // Overriden from WindowObserver:
  virtual void ShowStateChanged(aura::Window* window,
                                ui::WindowShowState last_show_state) OVERRIDE;

 private:
  WorkspaceManager2* workspace_manager();

  Workspace2* workspace_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager2);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER2_H_
