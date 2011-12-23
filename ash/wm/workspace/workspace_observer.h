// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_OBSERVER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace aura_shell {
namespace internal {
class Workspace;
class WorkspaceManager;

// A class to observe changes in workspace state.
class ASH_EXPORT WorkspaceObserver {
 public:
  // Invoked when |start| window is moved and inserted
  // at the |target| window's position by |WorkspaceManager::RotateWindow|.
  virtual void WindowMoved(WorkspaceManager* manager,
                           aura::Window* source,
                           aura::Window* target) = 0;

  // Invoked when the active workspace changes. |old| is
  // the old active workspace and can be NULL.
  virtual void ActiveWorkspaceChanged(WorkspaceManager* manager,
                                      Workspace* old) = 0;
 protected:
  virtual ~WorkspaceObserver() {}
};

}  // namespace internal
}  // namespace aura_shell

#endif  // ASH_WM_WORKSPACE_WORKSPACE_OBSERVER_H_
