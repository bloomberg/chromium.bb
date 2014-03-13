// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CAPTION_BUTTONS_BUBBLE_CONTENTS_BUTTON_ROW_H_
#define ASH_FRAME_CAPTION_BUTTONS_BUBBLE_CONTENTS_BUTTON_ROW_H_

#include "ash/wm/workspace/snap_types.h"
#include "ui/views/controls/button/button.h"

namespace views {
class CustomButton;
}

namespace ash {

class BubbleDialogButton;
class MaximizeBubbleControllerBubble;

// A class that creates all buttons and puts them into a view.
class BubbleContentsButtonRow : public views::View,
                                public views::ButtonListener {
 public:
  explicit BubbleContentsButtonRow(MaximizeBubbleControllerBubble* bubble);
  virtual ~BubbleContentsButtonRow();

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Called from BubbleDialogButton.
  void ButtonHovered(BubbleDialogButton* sender);

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

  MaximizeBubbleControllerBubble* bubble() { return bubble_; }

 private:
  // Functions to add the left and right maximize / restore buttons.
  void AddMaximizeLeftButton();
  void AddMaximizeRightButton();
  void AddMinimizeButton();

  // The owning object which gets notifications.
  MaximizeBubbleControllerBubble* bubble_;

  // The created buttons for our menu.
  BubbleDialogButton* left_button_;
  BubbleDialogButton* minimize_button_;
  BubbleDialogButton* right_button_;

  DISALLOW_COPY_AND_ASSIGN(BubbleContentsButtonRow);
};

}  // namespace ash

#endif  // ASH_FRAME_CAPTION_BUTTONS_BUBBLE_CONTENTS_BUTTON_ROW_H_
