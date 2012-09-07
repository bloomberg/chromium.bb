// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/tween.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace {
// Animation parameters.
const int kFrameRateHz = 60;
// Margins for animated box (pixels).
const int kTextMargin = 4;
const int kIconLeftMargin = 4;

}

LocationBarDecorationView::LocationBarDecorationView(
    LocationBarView* parent,
    const int background_images[])
    : parent_(parent),
      pause_animation_(false),
      pause_animation_state_(0.0),
      text_size_(0),
      visible_text_size_(0),
      force_draw_text_(false),
      background_painter_(background_images) {
  SetHorizontalAlignment(ImageView::LEADING);
  TouchableLocationBarView::Init(this);
}

LocationBarDecorationView::~LocationBarDecorationView() {
}

gfx::Size LocationBarDecorationView::GetPreferredSize() {
  gfx::Size preferred_size(views::ImageView::GetPreferredSize());
  preferred_size.set_height(std::max(preferred_size.height(),
                                     background_painter_.height()));
  // When view is animated |visible_text_size_| > 0, it is 0 otherwise.
  preferred_size.set_width(preferred_size.width() + visible_text_size_);
  return preferred_size;
}

ui::EventResult LocationBarDecorationView::OnGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP) {
    AnimationOnClick();
    OnClick(parent_);
    return ui::ER_CONSUMED;
  } else if (event.type() == ui::ET_GESTURE_TAP_DOWN) {
    return ui::ER_CONSUMED;
  }

  return ui::ER_UNHANDLED;
}

void LocationBarDecorationView::AnimationEnded(const ui::Animation* animation) {
  if (pause_animation_)
    pause_animation_ = false;
  slide_animator_->Reset();
}

void LocationBarDecorationView::AnimationProgressed(
    const ui::Animation* animation) {
  if (pause_animation_)
    return;

  visible_text_size_ = GetTextAnimationSize(slide_animator_->GetCurrentValue(),
                                            text_size_);

  parent_->Layout();
  parent_->SchedulePaint();
}

void LocationBarDecorationView::AnimationCanceled(
    const ui::Animation* animation) {
  AnimationEnded(animation);
}

int LocationBarDecorationView::GetBuiltInHorizontalPadding() const {
  return GetBuiltInHorizontalPaddingImpl();
}

void LocationBarDecorationView::StartLabelAnimation(string16 animated_text,
                                                    int duration_ms) {
  if (!slide_animator_.get()) {
    slide_animator_.reset(new ui::SlideAnimation(this));
    slide_animator_->SetSlideDuration(duration_ms);
    slide_animator_->SetTweenType(ui::Tween::LINEAR);
  }

  // Do not start animation if already in progress.
  if (!slide_animator_->is_animating()) {
    // Initialize animated string. It will be cleared when animation is
    // completed.
    animated_text_ = animated_text;
    text_size_ = ui::ResourceBundle::GetSharedInstance().GetFont(
        ui::ResourceBundle::MediumFont).GetStringWidth(animated_text);
    text_size_ += 2 * kTextMargin + kIconLeftMargin;
    slide_animator_->Show();
  }
}

int LocationBarDecorationView::GetTextAnimationSize(double state,
                                                    int text_size) {
  return text_size;
}

void LocationBarDecorationView::PauseAnimation() {
  if (pause_animation_)
    return;

  pause_animation_ = true;
  pause_animation_state_ = slide_animator_->GetCurrentValue();
}

void LocationBarDecorationView::UnpauseAnimation() {
  if (!pause_animation_)
    return;

  slide_animator_->Reset(pause_animation_state_);
  pause_animation_ = false;
  slide_animator_->Show();
}

void LocationBarDecorationView::AlwaysDrawText() {
  force_draw_text_ = true;
}

bool LocationBarDecorationView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationBarDecorationView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    return;

  AnimationOnClick();
  OnClick(parent_);
}

void LocationBarDecorationView::OnPaint(gfx::Canvas* canvas) {
  // During the animation we draw a border, an icon and the text. The text area
  // is changing in size during the animation, giving the appearance of the text
  // sliding out and then back in. When the text completely slid out the yellow
  // border is no longer painted around the icon. |visible_text_size_| is 0 when
  // animation is stopped.
  if (force_draw_text_ || (slide_animator_.get() &&
      (slide_animator_->is_animating() || pause_animation_))) {
    // In the non-animated state borders' left() is 0, in the animated state it
    // is the kIconLeftMargin, so we need to animate border reduction when it
    // starts to disappear.
    int necessary_left_margin = std::min(kIconLeftMargin, visible_text_size_);
    views::Border* empty_border =
        views::Border::CreateEmptyBorder(0, necessary_left_margin, 0, 0);
    set_border(empty_border);
    views::ImageView::OnPaint(canvas);

    // Paint text to the right of the icon.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    canvas->DrawStringInt(animated_text_,
        rb.GetFont(ui::ResourceBundle::MediumFont), SK_ColorBLACK,
        GetImageBounds().right() + kTextMargin, 0,
        width() - GetImageBounds().width(), height(),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::TEXT_VALIGN_MIDDLE);
  } else {
    views::ImageView::OnPaint(canvas);
  }
}

void LocationBarDecorationView::OnPaintBackground(gfx::Canvas* canvas) {
  if (force_draw_text_ || (slide_animator_.get() &&
      (slide_animator_->is_animating() || pause_animation_))) {
    background_painter_.Paint(canvas, size());
  } else {
    views::ImageView::OnPaintBackground(canvas);
  }
}

void LocationBarDecorationView::AnimationOnClick() {
  // Stop animation.
  if (slide_animator_.get() && slide_animator_->is_animating()) {
    PauseAnimation();
    slide_animator_->Reset();
  }
}
