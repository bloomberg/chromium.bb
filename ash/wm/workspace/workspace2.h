// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE2_H_
#define ASH_WM_WORKSPACE_WORKSPACE2_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {

class WorkspaceEventHandler;
class WorkspaceLayoutManager2;
class WorkspaceManager2;

// Workspace is used to maintain either a single maximized windows (including
// transients and other windows) or any number of windows (for the
// desktop). Workspace is used by WorkspaceManager to manage a set of windows.
class ASH_EXPORT Workspace2 {
 public:
  Workspace2(WorkspaceManager2* manager,
             aura::Window* parent,
             bool is_maximized);
  ~Workspace2();

  // Resets state. This should be used before destroying the Workspace.
  aura::Window* ReleaseWindow();

  bool is_maximized() const { return is_maximized_; }

  aura::Window* window() { return window_; }

  const WorkspaceLayoutManager2* workspace_layout_manager() const {
    return workspace_layout_manager_;
  }
  WorkspaceLayoutManager2* workspace_layout_manager() {
    return workspace_layout_manager_;
  }

  const WorkspaceManager2* workspace_manager() const {
    return workspace_manager_;
  }
  WorkspaceManager2* workspace_manager() { return workspace_manager_; }

  // Returns true if the Workspace should be moved to pending. This is true
  // if there are no visible maximized windows.
  bool ShouldMoveToPending() const;

  // Returns the number of maximized windows (including minimized windows that
  // would be maximized on restore). This does not consider visibility.
  int GetNumMaximizedWindows() const;

 private:
  // Is this a workspace for maximized windows?
  const bool is_maximized_;

  WorkspaceManager2* workspace_manager_;

  // Our Window, owned by |parent| passed to the constructor.
  aura::Window* window_;

  scoped_ptr<WorkspaceEventHandler> event_handler_;

  WorkspaceLayoutManager2* workspace_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(Workspace2);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE2_H_
