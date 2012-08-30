// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_BUBBLE_CONTROLLER_H_
#define ASH_WM_MAXIMIZE_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/maximize_bubble_frame_state.h"
#include "ash/wm/workspace/snap_types.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace base {
class Timer;
}

namespace views {
class CustomButton;
}

namespace ash {

class FrameMaximizeButton;

// A class which shows a helper UI for the maximize button after a delay.
class ASH_EXPORT MaximizeBubbleController {
 public:
  class Bubble;

  MaximizeBubbleController(FrameMaximizeButton* frame_maximize_button,
                           MaximizeBubbleFrameState maximize_type,
                           int appearance_delay_ms);
  // Called from the outside to destroy the interface to the UI visuals.
  // The visuals will then delete when possible (maybe asynchronously).
  virtual ~MaximizeBubbleController();

  // Update the UI visuals to reflect the previewed |snap_type| snapping state.
  void SetSnapType(SnapType snap_type);

  // To achieve proper Z-sorting with the snap animation, this window will be
  // presented above the phantom window.
  aura::Window* GetBubbleWindow();

  // Reset the delay of the menu creation (if it was not created yet).
  void DelayCreation();

  // Called to tell the owning FrameMaximizeButton that a button was clicked.
  void OnButtonClicked(SnapType snap_type);

  // Called to tell the the owning FrameMaximizeButton that the hover status
  // for a button has changed. |snap_type| can be either SNAP_LEFT, SNAP_RIGHT,
  // SNAP_MINIMIZE or SNAP_NONE.
  void OnButtonHover(SnapType snap_type);

  // Get the owning FrameMaximizeButton.
  FrameMaximizeButton* frame_maximize_button() {
    return frame_maximize_button_;
  }

  // The status of the associated window: Maximized or normal.
  MaximizeBubbleFrameState maximize_type() const { return maximize_type_; }

  // A unit test function to return buttons of the sub menu. |state| can be
  // either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

 protected:
  // Called from the the Bubble class to destroy itself: It tells the owning
  // object that it will destroy itself asynchronously. The owner will then
  // destroy |this|.
  void RequestDestructionThroughOwner();

 private:
  // The function which creates the bubble once the delay is elapsed.
  void CreateBubble();

  // The owning button which is also the anchor for the menu.
  FrameMaximizeButton* frame_maximize_button_;

  // The bubble menu.
  Bubble* bubble_;

  // The current maximize state of the owning window.
  const MaximizeBubbleFrameState maximize_type_;

  // The timer for the delayed creation of the menu.
  scoped_ptr<base::Timer> timer_;

  // The appearance delay in ms (delay and fade in & fade out delay).
  const int appearance_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeBubbleController);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_BUBBLE_CONTROLLER_H_
