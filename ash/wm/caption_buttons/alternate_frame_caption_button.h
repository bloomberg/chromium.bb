// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_CAPTION_BUTTON_H_
#define ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_CAPTION_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/custom_button.h"

namespace gfx {
class SlideAnimation;
}

namespace ash {

// Base class for buttons using the alternate button style.
class ASH_EXPORT AlternateFrameCaptionButton : public views::CustomButton {
 public:
  static const char kViewClassName[];

  enum Action {
    ACTION_MINIMIZE,
    ACTION_MAXIMIZE_RESTORE,
    ACTION_CLOSE
  };

  AlternateFrameCaptionButton(views::ButtonListener* listener, Action action);
  virtual ~AlternateFrameCaptionButton();

  // Returns the amount in pixels that the button should overlap with the button
  // on the left and right of it.
  static int GetXOverlap();

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Animates the background bubble for the current views::ButtonState.
  void MaybeStartNewBubbleAnimation();

  // views::CustomButton override:
  virtual void StateChanged() OVERRIDE;

  // ui::AnimateDelegate overrides. (views::CustomButton inherits from
  // gfx::AnimationDelegate).
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  Action action_;

  // The radius of the background bubble when it is hidden.
  double hidden_bubble_radius_;

  // The radius of the background bubble when it is visible.
  double shown_bubble_radius_;

  scoped_ptr<gfx::SlideAnimation> bubble_animation_;

  DISALLOW_COPY_AND_ASSIGN(AlternateFrameCaptionButton);
};

}  // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_ALTERNATE_FRAME_CAPTION_BUTTON_H_
