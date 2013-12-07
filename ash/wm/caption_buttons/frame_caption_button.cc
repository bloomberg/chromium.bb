// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/frame_caption_button.h"

#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// The duration of the crossfade animation when swapping the button's icon.
const int kCrossfadeDurationMs = 200;

}  // namespace

// static
const char FrameCaptionButton::kViewClassName[] = "FrameCaptionButton";

FrameCaptionButton::FrameCaptionButton(views::ButtonListener* listener,
                                       CaptionButtonIcon icon)
    : ImageButton(listener),
      icon_(icon),
      style_(STYLE_SHORT_RESTORED),
      last_paint_scale_(1.0f),
      animation_(new gfx::SlideAnimation(this)) {
  animation_->Reset(1);
  UpdateImages();
}

FrameCaptionButton::~FrameCaptionButton() {
}

void FrameCaptionButton::SetIcon(CaptionButtonIcon icon, Animate animate) {
  if (icon_ == icon)
    return;

  if (animate == ANIMATE_YES) {
    gfx::Canvas canvas(size(), last_paint_scale_, false);
    OnPaint(&canvas);
    crossfade_image_ = gfx::ImageSkia(canvas.ExtractImageRep());

    icon_ = icon;
    UpdateImages();

    animation_->Reset(0);
    animation_->SetSlideDuration(kCrossfadeDurationMs);
    animation_->Show();
  } else {
    animation_->Reset(1);
    icon_ = icon;
    UpdateImages();
  }
}

void FrameCaptionButton::SetStyle(Style style) {
  if (style_ == style)
    return;
  animation_->Reset(1);
  style_ = style;
  UpdateImages();
}

const char* FrameCaptionButton::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButton::OnPaint(gfx::Canvas* canvas) {
  last_paint_scale_ = canvas->image_scale();
  int alpha = static_cast<int>(animation_->GetCurrentValue() * 255);
  int crossfade_alpha = 255 - alpha;
  if (crossfade_alpha > 0 && !crossfade_image_.isNull()) {
    gfx::Canvas composed_canvas(size(), last_paint_scale_, false);
    SkPaint paint;
    paint.setAlpha(crossfade_alpha);
    paint.setXfermodeMode(SkXfermode::kPlus_Mode);
    composed_canvas.DrawImageInt(crossfade_image_, 0, 0, paint);
    paint.setAlpha(alpha);
    ImageButton::OnPaint(&composed_canvas);

    canvas->DrawImageInt(
        gfx::ImageSkia(composed_canvas.ExtractImageRep()), 0, 0);
  } else {
    ImageButton::OnPaint(canvas);
  }
}

void FrameCaptionButton::OnGestureEvent(ui::GestureEvent* event) {
  // ImageButton does not become pressed when the user drags off and then back
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
  ImageButton::OnGestureEvent(event);
}

void FrameCaptionButton::StateChanged() {
  if (state_ == STATE_HOVERED || state_ == STATE_PRESSED)
    animation_->Reset(1);
}

void FrameCaptionButton::UpdateImages() {
  switch (icon_) {
    case CAPTION_BUTTON_ICON_MINIMIZE:
      SetImages(IDR_AURA_WINDOW_MINIMIZE_SHORT,
                IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
                IDR_AURA_WINDOW_MINIMIZE_SHORT_P);
      break;
   case CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
   case CAPTION_BUTTON_ICON_LEFT_SNAPPED:
   case CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
     if (style_ == STYLE_SHORT_MAXIMIZED_OR_FULLSCREEN) {
       SetImages(IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
                 IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
                 IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
     } else if (style_ == STYLE_SHORT_RESTORED) {
       SetImages(IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                 IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                 IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
     } else {
       SetImages(IDR_AURA_WINDOW_MAXIMIZE,
                 IDR_AURA_WINDOW_MAXIMIZE_H,
                 IDR_AURA_WINDOW_MAXIMIZE_P);
     }
     break;
   case CAPTION_BUTTON_ICON_CLOSE:
     if (style_ == STYLE_SHORT_MAXIMIZED_OR_FULLSCREEN) {
       SetImages(IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
                 IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
                 IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P);
     } else if (style_ == STYLE_SHORT_RESTORED) {
       SetImages(IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                 IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                 IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
     } else {
       SetImages(IDR_AURA_WINDOW_CLOSE,
                 IDR_AURA_WINDOW_CLOSE_H,
                 IDR_AURA_WINDOW_CLOSE_P);
     }
     break;
  }

  SchedulePaint();
}

void FrameCaptionButton::SetImages(int normal_image_id,
                                   int hot_image_id,
                                   int pushed_image_id) {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(STATE_NORMAL, resource_bundle.GetImageSkiaNamed(normal_image_id));
  SetImage(STATE_HOVERED, resource_bundle.GetImageSkiaNamed(hot_image_id));
  SetImage(STATE_PRESSED, resource_bundle.GetImageSkiaNamed(pushed_image_id));
}

void FrameCaptionButton::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

}  // namespace ash
