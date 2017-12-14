// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CLIENT_CONTROLLED_STATE_H_
#define ASH_WM_CLIENT_CONTROLLED_STATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/base_state.h"
#include "ash/wm/wm_event.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace mojom {
enum class WindowStateType;
}

namespace wm {

// ClientControlledState delegates the window state transition and
// bounds control to the client. Its window state and bounds are
// determined by the delegate. ARC++ window's state is controlled by
// Android framework, for example.
class ASH_EXPORT ClientControlledState : public BaseState {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Handles the state change of |window_state| to |requested_state|.
    // Delegate may decide to ignore the state change, proceed with the state
    // change, or can move to a different state.
    virtual void HandleWindowStateRequest(
        WindowState* window_state,
        mojom::WindowStateType requested_state) = 0;
    // Handles the bounds change request for |window_state|.  Delegate
    // may choose to ignore the request, set the given bounds, or set
    // the different bounds.
    virtual void HandleBoundsRequest(WindowState* window_state,
                                     const gfx::Rect& requested_bounds) = 0;
  };

  explicit ClientControlledState(std::unique_ptr<Delegate> delegate);
  ~ClientControlledState() override;

  // A flag used to update the window's bounds directly, instead of
  // delegating to |Delegate|. The Delegate should use this to
  // apply the bounds change to the window.
  void set_bounds_locally(bool set) { set_bounds_locally_ = set; }

  // Type of animation type to be applied when changing bounds locally.
  // TODO(oshima): Use transform animation for snapping.
  enum BoundsChangeAnimationType {
    kAnimationNone,
    kAnimationCrossFade,
  };

  // WindowState::State:
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override;
  void DetachState(WindowState* window_state) override;

  // BaseState:
  void HandleWorkspaceEvents(WindowState* window_state,
                             const WMEvent* event) override;
  void HandleCompoundEvents(WindowState* window_state,
                            const WMEvent* event) override;
  void HandleBoundsEvents(WindowState* window_state,
                          const WMEvent* event) override;
  void HandleTransitionEvents(WindowState* window_state,
                              const WMEvent* event) override;

  // Enters next state. This is used when the state moves from one to another
  // within the same desktop mode. Returns true if the state has changed, or
  // false otherwise.
  // |animation_type| specifies the type of animation to be applied when
  // bounds changes.
  bool EnterNextState(wm::WindowState* window_state,
                      mojom::WindowStateType next_state_type,
                      BoundsChangeAnimationType animation_type);

 private:
  std::unique_ptr<Delegate> delegate_;

  bool set_bounds_locally_ = false;

  BoundsChangeAnimationType bounds_change_animation_type_ = kAnimationNone;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledState);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_DEFAULT_STATE_H_
