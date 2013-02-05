// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_cycler_animator.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/events/event_handler.h"

namespace ash {
namespace internal {

class WorkspaceManager;

// Class to enable quick workspace switching (cycling) via 3 finger vertical
// scroll.
// During cycling, the workspaces are arranged in a card stack as shown below:
//  ____________________________________________
// |          ________________________          |
// |         |                        |         |
// |        _|________________________|_        |
// |       |                            |       |
// |      _|____________________________|_      |
// |     |                                |     |
// |     |                                |     |
// |     |     workspace to activate      |     |
// |     |                                |     |
// |     |                                |     |
// |    _|________________________________|_    |
// |   |                                    |   |
// |  _|____________________________________|_  |
// | |                                        | |
// |_|________________________________________|_|
// The user selects a workspace to activate by moving the workspace they want
// to activate to the most prominent position in the card stack.

class ASH_EXPORT WorkspaceCycler : public ui::EventHandler,
                                   public WorkspaceCyclerAnimator::Delegate {
 public:
  explicit WorkspaceCycler(WorkspaceManager* workspace_manager);
  virtual ~WorkspaceCycler();

  // Abort any animations that the cycler is doing. The active workspace is set
  // to the active workspace before cycling was initiated.
  // Cycling should be aborted:
  // - Before a workspace is added or destroyed.
  // - Before a workspace is activated.
  // - When the workspace bounds or the shelf bounds change.
  void AbortCycling();

 private:
  // The cycler state.
  enum State {
    NOT_CYCLING,

    // The cycler is waiting for the user to scroll far enough vertically to
    // trigger cycling workspaces.
    NOT_CYCLING_TRACKING_SCROLL,

    // The workspaces are animating into a card stack configuration. The cycler
    // is in this state for the duration of the animation.
    STARTING_CYCLING,

    // The user is moving workspaces around in the card stack.
    CYCLING,

    // The workspace that the user selected to be active is being animated to
    // take up the entire screen. The cycler is in this state for the duration
    // of the animation.
    STOPPING_CYCLING,
  };

  // Set the cycler state to |new_state|.
  // The state is not changed if transitioning from the current state to
  // |new_state| is not valid.
  void SetState(State new_state);

  // Returns true if transitioning from |state_| to |next_state| is valid.
  bool IsValidNextState(State next_state) const;

  // ui::EventHandler overrides:
  virtual void OnEvent(ui::Event* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // WorkspaceCyclerAnimator::Delegate overrides:
  virtual void StartWorkspaceCyclerAnimationFinished() OVERRIDE;
  virtual void StopWorkspaceCyclerAnimationFinished() OVERRIDE;

  WorkspaceManager* workspace_manager_;

  scoped_ptr<WorkspaceCyclerAnimator> animator_;

  // The cycler's state.
  State state_;

  // The amount of scrolling which has occurred.
  float scroll_x_;
  float scroll_y_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceCycler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_CYCLER_H_
