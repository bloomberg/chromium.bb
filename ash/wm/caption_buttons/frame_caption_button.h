// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
#define ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/caption_buttons/caption_button_types.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"

namespace gfx {
class SlideAnimation;
}

namespace ash {

// Base class for the window caption buttons (minimize, maximize, restore,
// close).
class ASH_EXPORT FrameCaptionButton : public views::CustomButton {
 public:
  enum Animate {
    ANIMATE_YES,
    ANIMATE_NO
  };

  static const char kViewClassName[];

  FrameCaptionButton(views::ButtonListener* listener, CaptionButtonIcon icon);
  virtual ~FrameCaptionButton();

  // Sets the images to use to paint the button. If |animate| is ANIMATE_YES,
  // the button crossfades to the new visuals. If the image ids match those
  // currently used by the button and |animate| is ANIMATE_NO the crossfade
  // animation is progressed to the end.
  void SetImages(CaptionButtonIcon icon,
                 Animate animate,
                 int icon_image_id,
                 int inactive_icon_image_id,
                 int hovered_background_image_id,
                 int pressed_background_image_id);

  // Returns true if the button is crossfading to new visuals set in
  // SetImages().
  bool IsAnimatingImageSwap() const;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  void set_paint_as_active(bool paint_as_active) {
    paint_as_active_ = paint_as_active;
  }

  CaptionButtonIcon icon() const {
    return icon_;
  }

 protected:
  // views::CustomButton overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void StateChanged() OVERRIDE;

 private:
  // Paints the button as it will look at the end of |swap_images_animation_|
  // but with |opacity|.
  void PaintWithAnimationEndState(gfx::Canvas* canvas, int opacity);

  // The button's current icon.
  CaptionButtonIcon icon_;

  // Whether the button should be painted as active.
  bool paint_as_active_;

  // The scale at which the button was previously painted.
  float last_paint_scale_;

  // The images and image ids used to paint the button.
  int icon_image_id_;
  int inactive_icon_image_id_;
  int hovered_background_image_id_;
  int pressed_background_image_id_;
  gfx::ImageSkia icon_image_;
  gfx::ImageSkia inactive_icon_image_;
  gfx::ImageSkia hovered_background_image_;
  gfx::ImageSkia pressed_background_image_;

  // The image to crossfade from.
  gfx::ImageSkia crossfade_image_;

  // Crossfade animation started when the button's images are changed by
  // SetImages().
  scoped_ptr<gfx::SlideAnimation> swap_images_animation_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButton);
};

}  // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_H_
