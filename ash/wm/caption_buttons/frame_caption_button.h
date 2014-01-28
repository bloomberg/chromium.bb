// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
#define ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/caption_buttons/caption_button_types.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
class SlideAnimation;
}

namespace ash {

// Base class for the window caption buttons (minimize, maximize, restore,
// close).
class ASH_EXPORT FrameCaptionButton : public views::ImageButton {
 public:
  enum Animate {
    ANIMATE_YES,
    ANIMATE_NO
  };

  enum Style {
    // Restored browser windows.
    STYLE_TALL_RESTORED,

    // All other restored windows.
    STYLE_SHORT_RESTORED,

    // Maximized or fullscreen windows.
    STYLE_SHORT_MAXIMIZED_OR_FULLSCREEN
  };

  static const char kViewClassName[];

  FrameCaptionButton(views::ButtonListener* listener, CaptionButtonIcon icon);
  virtual ~FrameCaptionButton();

  // Sets the button's icon. If |animate| is ANIMATE_YES, the button crossfades
  // to the new icon.
  void SetIcon(CaptionButtonIcon icon, Animate animate);

  // Sets the button's style. The transition to the new style is not animated.
  void SetStyle(Style style);

  // views::View overrides:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  CaptionButtonIcon icon() const {
    return icon_;
  }

 protected:
  // views::CustomButton overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void StateChanged() OVERRIDE;

 private:
  // Paints the button as it will look at the end of the animation but with
  // |opacity|.
  void PaintWithAnimationEndState(gfx::Canvas* canvas, int opacity);

  // Updates the button's images based on the current icon and style.
  void UpdateImages();

  // Sets the button's images based on the given ids.
  void SetImages(int normal_image_id, int hot_image_id, int pushed_image_id);

  // gfx::AnimationDelegate override:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // The button's current icon.
  CaptionButtonIcon icon_;

  // The button's current style.
  Style style_;

  // The scale at which the button was previously painted.
  float last_paint_scale_;

  // The image to crossfade from.
  gfx::ImageSkia crossfade_image_;

  scoped_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButton);
};

}  // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
