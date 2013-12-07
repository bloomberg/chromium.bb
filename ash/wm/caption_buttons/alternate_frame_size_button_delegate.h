// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_SIZE_BUTTON_DELEGATE_H_
#define ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_SIZE_BUTTON_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/wm/caption_buttons/caption_button_types.h"

namespace gfx {
class Insets;
class Point;
}

namespace ash {
class FrameCaptionButton;

// Delegate interface for AlternateFrameSizeButton.
class ASH_EXPORT AlternateFrameSizeButtonDelegate {
 public:
  enum Animate {
    ANIMATE_YES,
    ANIMATE_NO
  };

  // Returns whether the minimize button is visible.
  virtual bool IsMinimizeButtonVisible() const = 0;

  // Reset the caption button views::Button::ButtonState back to normal. If
  // |animate| is ANIMATE_YES, the buttons will crossfade back to their
  // original icons.
  virtual void SetButtonsToNormal(Animate animate) = 0;

  // Sets the minimize and close button icons. The buttons will crossfade to
  // their new icons if |animate| is ANIMATE_YES.
  virtual void SetButtonIcons(CaptionButtonIcon left_button_action,
                              CaptionButtonIcon right_button_action,
                              Animate animate) = 0;

  // Presses the button at |position_in_screen| and unpresses any other pressed
  // caption buttons.
  // |pressed_button_hittest_insets| indicates how much the hittest insets for
  // the currently pressed button should be expanded if no button was found at
  // |position_in_screen| using the normal button hittest insets.
  // Returns the button which was pressed.
  virtual const FrameCaptionButton* PressButtonAt(
      const gfx::Point& position_in_screen,
      const gfx::Insets& pressed_button_hittest_insets) const = 0;

 protected:
  virtual ~AlternateFrameSizeButtonDelegate() {}
};

}  // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_SIZE_BUTTON_DELEGATE_H_
