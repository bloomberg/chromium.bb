// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_CYCLER_ANIMATOR_H_
#define ASH_WM_WORKSPACE_WORKSPACE_CYCLER_ANIMATOR_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace gfx {
class Transform;
}

namespace ash {
namespace internal {
class ColoredWindowController;
class StyleCalculator;
class Workspace;

// Class which manages the animations for cycling through workspaces.
class ASH_EXPORT WorkspaceCyclerAnimator :
    public base::SupportsWeakPtr<WorkspaceCyclerAnimator>,
    public ui::ImplicitAnimationObserver {
 public:
  // Class used for notifying when the workspace cycler has finished animating
  // starting and stopping the cycler.
  class ASH_EXPORT Delegate {
   public:
    // Called when the animations initiated by AnimateStartingCycler() are
    // completed.
    virtual void StartWorkspaceCyclerAnimationFinished() = 0;

    // Called when the animations initiated by AnimateStoppingCycler() are
    // completed. This is not called as a result of AbortAnimations().
    virtual void StopWorkspaceCyclerAnimationFinished() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit WorkspaceCyclerAnimator(Delegate* delegate);
  virtual ~WorkspaceCyclerAnimator();

  // Initializes the animator with the passed in state. The animator caches all
  // of the parameters.
  // AbortAnimations() should be called:
  // - Before a workspace is added or destroyed.
  // - Before a workspace is activated.
  // - When the workspace bounds or the shelf bounds change.
  void Init(const std::vector<Workspace*>& workspaces,
            Workspace* initial_active_workspace);

  // Animate starting the workspace cycler.
  // StartWorkspaceCyclerAnimationFinished() will be called on the delegate when
  // the animations have completed.
  void AnimateStartingCycler();

  // Animate stopping the workspace cycler.
  // StopWorkspaceCyclerAnimationFinished() will be called on the delegate when
  // the animations have completed.
  void AnimateStoppingCycler();

  // Abort the animations started by the animator and reset any state set by the
  // animator.
  void AbortAnimations();

  // Animate cycling by |scroll_delta|.
  void AnimateCyclingByScrollDelta(double scroll_delta);

  // Returns the workspace which should be activated if the user does not do
  // any more cycling.
  Workspace* get_selected_workspace() const {
    return workspaces_[selected_workspace_index_];
  }

 private:
  enum AnimationType {
    NONE,
    CYCLER_START,
    CYCLER_UPDATE,
    CYCLER_COMPLETELY_SELECT,
    CYCLER_END
  };

  // Start animations of |animation_duration| to the state dictated by
  // |selected_workspace_index_|, |scroll_delta_| and |animation_type_|.
  void AnimateToUpdatedState(int animation_duration);

  // Called after the animations, if any, have occurred for stopping / aborting
  // cycling. Resets any state set by the animator.
  // The workspace at |visible_workspace_index| is set as the only visible
  // workspace.
  void CyclerStopped(size_t visible_workspace_index);

  // Start an animation of |animation_duration| of the workspace's properties to
  // the passed in target values.
  // If |wait_for_animation_to_complete| is set to true, the animations for all
  // of the workspaces are considered completed and the animation callback is
  // called when the animation for the workspace at |workpace_index| has
  // completed. |wait_for_animations_to_complete| can be set on a single
  // workspace. If workspaces have animations of different durations, this
  // property should be set on the workspace with the longest animation.
  void AnimateTo(size_t workspace_index,
                 bool wait_for_animation_to_complete,
                 int animation_duration,
                 const gfx::Transform& target_transform,
                 float target_brightness,
                 bool target_visibility);

  // Returns the animation duration of cycing given |change|, the change in
  // scroll delta.
  int GetAnimationDurationForChangeInScrollDelta(double change) const;

  // Create a black window and place it behind the launcher. This simulates a
  // fully opaque launcher background.
  void CreateLauncherBackground();

  // Returns the desktop background window.
  aura::Window* GetDesktopBackground() const;

  // Notify the |delegate| about |completed_animation|.
  void NotifyDelegate(AnimationType completed_animation);

  // Overridden from ui::ImplicitAnimationObserver.
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // The delegate to be notified when the animator has finished animating
  // starting or stopping the cycler.
  Delegate* delegate_;

  // Cache of the unminimized workspaces.
  std::vector<Workspace*> workspaces_;

  // The bounds of the display containing the workspaces in workspace
  // coordinates, including the shelf if any.
  gfx::Rect screen_bounds_;

  // The bounds of a maximized window. This excludes the shelf if any.
  gfx::Rect maximized_bounds_;

  // The index of the active workspace when the cycler was constructed.
  size_t initial_active_workspace_index_;

  // The index of the workspace which should be activated if the user does not
  // do any more cycling.
  size_t selected_workspace_index_;

  // The amount that the user has scrolled vertically in pixels since selecting
  // the workspace at |selected_workspace_index_|.
  // Values:
  // 0
  //  The workspace at |selected_workspace_index_| is completely selected.
  // Positive
  //  The user is part of the way to selecting the workspace at
  //  |selected_workspace_index_ + 1|.
  // Negative
  //  The user is part of the way to selecting the workspace at
  //  |selected_workspace_index_ - 1|.
  double scroll_delta_;

  // The type of the current animation. |animation_type_| is NONE when the
  // current animation is finished and a new one can be started.
  AnimationType animation_type_;

  // The window controller for the fake black launcher background.
  scoped_ptr<ColoredWindowController> launcher_background_controller_;

  // Used by the animator to compute the transform, brightness, and visibility
  // of workspaces during animations.
  scoped_ptr<StyleCalculator> style_calculator_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceCyclerAnimator);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_CYCLER_ANIMATOR_H_
