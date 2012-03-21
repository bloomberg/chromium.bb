// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MANAGED_WORKSPACE_H_
#define ASH_WM_WORKSPACE_MANAGED_WORKSPACE_H_

#include "ash/wm/workspace/workspace.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

// ManagedWorkspace allows any non-maximized/fullscreen windows. It imposes
// constraints on where the windows can be placed and how they can be resized.
class ASH_EXPORT ManagedWorkspace : public Workspace {
 public:
  explicit ManagedWorkspace(WorkspaceManager* manager);
  virtual ~ManagedWorkspace();

 protected:
  // Workspace overrides:
  virtual bool CanAdd(aura::Window* window) const OVERRIDE;
  virtual void OnWindowAddedAfter(aura::Window* window,
                                  aura::Window* after) OVERRIDE;
  virtual void OnWindowRemoved(aura::Window* window) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedWorkspace);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MANAGED_WORKSPACE_H_
