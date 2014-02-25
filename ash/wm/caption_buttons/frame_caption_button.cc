// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/frame_caption_button.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// The duration of the crossfade animation when swapping the button's images.
const int kSwapImagesAnimationDurationMs = 200;

// The duration of the fade out animation of the old icon during a crossfade
// animation as a ratio of |kSwapImagesAnimationDurationMs|.
const float kFadeOutRatio = 0.5f;

}  // namespace

// static
const char FrameCaptionButton::kViewClassName[] = "FrameCaptionButton";

FrameCaptionButton::FrameCaptionButton(views::ButtonListener* listener,
                                       CaptionButtonIcon icon)
    : CustomButton(listener),
      icon_(icon),
      last_paint_scale_(1.0f),
      normal_image_id_(-1),
      hovered_image_id_(-1),
      pressed_image_id_(-1),
      swap_images_animation_(new gfx::SlideAnimation(this)) {
  swap_images_animation_->Reset(1);
  EnableCanvasFlippingForRTLUI(true);
}

FrameCaptionButton::~FrameCaptionButton() {
}

void FrameCaptionButton::SetImages(CaptionButtonIcon icon,
                                   Animate animate,
                                   int normal_image_id,
                                   int hovered_image_id,
                                   int pressed_image_id) {
  // The early return is dependant on |animate| because callers use SetImages()
  // with ANIMATE_NO to progress the crossfade animation to the end.
  if (icon == icon_ &&
      (animate == ANIMATE_YES || !swap_images_animation_->is_animating()) &&
      normal_image_id == normal_image_id_ &&
      hovered_image_id == hovered_image_id_ &&
      pressed_image_id == pressed_image_id_) {
    return;
  }

  if (animate == ANIMATE_YES) {
    gfx::Canvas canvas(size(), last_paint_scale_, false);
    OnPaint(&canvas);
    crossfade_image_ = gfx::ImageSkia(canvas.ExtractImageRep());
  }

  icon_ = icon;
  normal_image_id_ = normal_image_id;
  hovered_image_id_ = hovered_image_id;
  pressed_image_id_ = pressed_image_id;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  normal_image_ = *rb.GetImageSkiaNamed(normal_image_id);
  hovered_image_ = *rb.GetImageSkiaNamed(hovered_image_id);
  pressed_image_ = *rb.GetImageSkiaNamed(pressed_image_id);

  if (animate == ANIMATE_YES) {
    swap_images_animation_->Reset(0);
    swap_images_animation_->SetSlideDuration(kSwapImagesAnimationDurationMs);
    swap_images_animation_->Show();
  } else {
    swap_images_animation_->Reset(1);
  }
  PreferredSizeChanged();
  SchedulePaint();
}

bool FrameCaptionButton::IsAnimatingImageSwap() const {
  return swap_images_animation_->is_animating();
}

gfx::Size FrameCaptionButton::GetPreferredSize() {
  return normal_image_.isNull() ? gfx::Size() : normal_image_.size();
}

const char* FrameCaptionButton::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButton::OnPaint(gfx::Canvas* canvas) {
  // TODO(pkotwicz): Take |CustomButton::hover_animation_| into account once
  // the button has separate images for the icon and background.

  last_paint_scale_ = canvas->image_scale();
  double animation_value = swap_images_animation_->GetCurrentValue();
  int alpha = static_cast<int>(animation_value * 255);
  int crossfade_alpha = 0;
  if (animation_value < kFadeOutRatio) {
     crossfade_alpha = static_cast<int>(
         255 * (1 - animation_value / kFadeOutRatio));
  }
  if (crossfade_alpha > 0 && !crossfade_image_.isNull()) {
    gfx::Canvas composed_canvas(size(), last_paint_scale_, false);
    PaintWithAnimationEndState(&composed_canvas, alpha);

    SkPaint paint;
    paint.setAlpha(crossfade_alpha);
    paint.setXfermodeMode(SkXfermode::kPlus_Mode);
    composed_canvas.DrawImageInt(crossfade_image_, 0, 0, paint);

    canvas->DrawImageInt(
        gfx::ImageSkia(composed_canvas.ExtractImageRep()), 0, 0);
  } else {
    PaintWithAnimationEndState(canvas, alpha);
  }
}

void FrameCaptionButton::OnGestureEvent(ui::GestureEvent* event) {
  // CustomButton does not become pressed when the user drags off and then back
  // onto the button. Make FrameCaptionButton pressed in this case because this
  // behavior is more consistent with AlternateFrameSizeButton.
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_PRESSED);
      RequestFocus();
      event->StopPropagation();
    } else {
      SetState(STATE_NORMAL);
    }
  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_HOVERED);
      NotifyClick(*event);
      event->StopPropagation();
    }
  }
  CustomButton::OnGestureEvent(event);
}

void FrameCaptionButton::StateChanged() {
  if (state_ == STATE_HOVERED || state_ == STATE_PRESSED)
    swap_images_animation_->Reset(1);
}

void FrameCaptionButton::PaintWithAnimationEndState(
    gfx::Canvas* canvas,
    int opacity) {
  gfx::ImageSkia img;
  if (state() == STATE_HOVERED) {
    img = hovered_image_;
  } else if (state() == STATE_PRESSED) {
    img = pressed_image_;
  } else {
    img = normal_image_;
  }

  if (!img.isNull()) {
    SkPaint paint;
    paint.setAlpha(opacity);
    canvas->DrawImageInt(img, 0, 0, paint);
  }
}

}  // namespace ash
