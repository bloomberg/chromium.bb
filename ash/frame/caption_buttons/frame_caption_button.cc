// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/frame_caption_button.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/throb_animation.h"
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
      paint_as_active_(false),
      alpha_(255),
      icon_image_id_(-1),
      inactive_icon_image_id_(-1),
      hovered_background_image_id_(-1),
      pressed_background_image_id_(-1),
      swap_images_animation_(new gfx::SlideAnimation(this)) {
  swap_images_animation_->Reset(1);

  // Do not flip the gfx::Canvas passed to the OnPaint() method. The snap left
  // and snap right button icons should not be flipped. The other icons are
  // horizontally symmetrical.
}

FrameCaptionButton::~FrameCaptionButton() {
}

void FrameCaptionButton::SetImages(CaptionButtonIcon icon,
                                   Animate animate,
                                   int icon_image_id,
                                   int inactive_icon_image_id,
                                   int hovered_background_image_id,
                                   int pressed_background_image_id) {
  // The early return is dependant on |animate| because callers use SetImages()
  // with ANIMATE_NO to progress the crossfade animation to the end.
  if (icon == icon_ &&
      (animate == ANIMATE_YES || !swap_images_animation_->is_animating()) &&
      icon_image_id == icon_image_id_ &&
      inactive_icon_image_id == inactive_icon_image_id_ &&
      hovered_background_image_id == hovered_background_image_id_ &&
      pressed_background_image_id == pressed_background_image_id_) {
    return;
  }

  if (animate == ANIMATE_YES)
    crossfade_icon_image_ = GetIconImageToPaint();

  icon_ = icon;
  icon_image_id_ = icon_image_id;
  inactive_icon_image_id_ = inactive_icon_image_id;
  hovered_background_image_id_ = hovered_background_image_id;
  pressed_background_image_id_ = pressed_background_image_id;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_image_ = *rb.GetImageSkiaNamed(icon_image_id);
  inactive_icon_image_ = *rb.GetImageSkiaNamed(inactive_icon_image_id);
  hovered_background_image_ = *rb.GetImageSkiaNamed(
      hovered_background_image_id);
  pressed_background_image_ = *rb.GetImageSkiaNamed(
      pressed_background_image_id);

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

void FrameCaptionButton::SetAlpha(int alpha) {
  if (alpha_ != alpha) {
    alpha_ = alpha;
    SchedulePaint();
  }
}

gfx::Size FrameCaptionButton::GetPreferredSize() const {
  return hovered_background_image_.isNull() ?
      gfx::Size() : hovered_background_image_.size();
}

const char* FrameCaptionButton::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButton::OnPaint(gfx::Canvas* canvas) {
  if (hover_animation_->is_animating() || state() == STATE_HOVERED) {
    int hovered_background_alpha = hover_animation_->is_animating() ?
        hover_animation_->CurrentValueBetween(0, 255) : 255;
    SkPaint paint;
    paint.setAlpha(hovered_background_alpha);
    canvas->DrawImageInt(hovered_background_image_, 0, 0, paint);
  } else if (state() == STATE_PRESSED) {
    canvas->DrawImageInt(pressed_background_image_, 0, 0);
  }

  int icon_alpha = swap_images_animation_->CurrentValueBetween(0, 255);
  int crossfade_icon_alpha = 0;
  if (icon_alpha < static_cast<int>(kFadeOutRatio * 255))
     crossfade_icon_alpha = static_cast<int>(255 - icon_alpha / kFadeOutRatio);

  gfx::ImageSkia icon_image = GetIconImageToPaint();
  if (crossfade_icon_alpha > 0 && !crossfade_icon_image_.isNull()) {
    gfx::Canvas icon_canvas(icon_image.size(), canvas->image_scale(), false);
    SkPaint paint;
    paint.setAlpha(icon_alpha);
    icon_canvas.DrawImageInt(icon_image, 0, 0, paint);

    paint.setAlpha(crossfade_icon_alpha);
    paint.setXfermodeMode(SkXfermode::kPlus_Mode);
    icon_canvas.DrawImageInt(crossfade_icon_image_, 0, 0, paint);

    PaintCentered(canvas, gfx::ImageSkia(icon_canvas.ExtractImageRep()),
                  alpha_);
  } else {
    if (!swap_images_animation_->is_animating())
      icon_alpha = alpha_;
    PaintCentered(canvas, icon_image, icon_alpha);
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

const gfx::ImageSkia& FrameCaptionButton::GetIconImageToPaint() const {
  return paint_as_active_ ? icon_image_ : inactive_icon_image_;
}

void FrameCaptionButton::PaintCentered(gfx::Canvas* canvas,
                                       const gfx::ImageSkia& to_center,
                                       int alpha) {
  SkPaint paint;
  paint.setAlpha(alpha);
  canvas->DrawImageInt(to_center,
                       (width() - to_center.width()) / 2,
                       (height() - to_center.height()) / 2,
                       paint);
}

}  // namespace ash
