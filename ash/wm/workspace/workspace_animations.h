// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_ANIMATIONS_H_
#define ASH_WM_WORKSPACE_WORKSPACE_ANIMATIONS_H_

#include "ash/ash_export.h"
#include "base/time.h"

namespace aura {
class Window;
}

// Collection of functions and types needed for animating workspaces.

namespace ash {
namespace internal {

// Indicates the direction the workspace should appear to go.
enum WorkspaceAnimationDirection {
  WORKSPACE_ANIMATE_UP,
  WORKSPACE_ANIMATE_DOWN,
};

// The details of this dictate how the show/hide should be animated.
struct WorkspaceAnimationDetails {
  WorkspaceAnimationDetails();
  ~WorkspaceAnimationDetails();

  // Direction to animate.
  WorkspaceAnimationDirection direction;

  // Whether to animate. If false the show/hide is immediate, otherwise it
  // animates.
  bool animate;

  // Whether the opacity should be animated.
  bool animate_opacity;

  // Whether the scale should be animated.
  bool animate_scale;

  // The duration of the animation. If empty the default is used.
  base::TimeDelta duration;

  // Amount of time (in milliseconds) to pause before animating.
  int pause_time_ms;
};

// Amount of time for the workspace switch animation.
extern const int kWorkspaceSwitchTimeMS;

// Shows or hides the workspace animating based on |details|.
ASH_EXPORT void ShowWorkspace(aura::Window* window,
                              const WorkspaceAnimationDetails& details);
ASH_EXPORT void HideWorkspace(aura::Window* window,
                              const WorkspaceAnimationDetails& details);
}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_ANIMATIONS_H_
