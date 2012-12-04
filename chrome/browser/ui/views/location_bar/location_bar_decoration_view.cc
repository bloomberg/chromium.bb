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
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {
// Animation parameters.
const int kFrameRateHz = 60;
// Margins for animated box (pixels).
const int kHorizMargin = 4;
const int kIconLabelSpacing = 4;
}

LocationBarDecorationView::LocationBarDecorationView(
    LocationBarView* parent,
    const int background_images[],
    const gfx::Font& font,
    SkColor font_color)
    : parent_(parent),
      text_label_(NULL),
      icon_(new views::ImageView),
      font_(font),
      font_color_(font_color),
      pause_animation_(false),
      pause_animation_state_(0.0),
      text_size_(0),
      visible_text_size_(0),
      force_draw_text_(false),
      background_painter_(background_images) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  icon_->SetHorizontalAlignment(views::ImageView::LEADING);
  AddChildView(icon_);
  TouchableLocationBarView::Init(this);
}

LocationBarDecorationView::~LocationBarDecorationView() {
}

void LocationBarDecorationView::SetImage(const gfx::ImageSkia* image_skia) {
  icon_->SetImage(image_skia);
}

void LocationBarDecorationView::SetTooltipText(const string16& tooltip) {
  icon_->SetTooltipText(tooltip);
}

gfx::Size LocationBarDecorationView::GetPreferredSize() {
  gfx::Size preferred_size(views::View::GetPreferredSize());
  preferred_size.set_height(std::max(preferred_size.height(),
                                     background_painter_.height()));
  int non_label_width = preferred_size.width() -
      (text_label_ ? text_label_->GetPreferredSize().width() : 0);
  // When view is animated |visible_text_size_| > 0, it is 0 otherwise.
  preferred_size.set_width(non_label_width + visible_text_size_);
  return preferred_size;
}

void LocationBarDecorationView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    AnimationOnClick();
    OnClick(parent_);
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    event->SetHandled();
  }
}

void LocationBarDecorationView::AnimationEnded(const ui::Animation* animation) {
  if (!pause_animation_ && !force_draw_text_) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    RemoveChildView(text_label_);  // will also delete the view.
    text_label_ = NULL;
    parent_->Layout();
    parent_->SchedulePaint();
  }
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
    if (!text_label_) {
      text_label_ = new views::Label;
      text_label_->SetElideBehavior(views::Label::NO_ELIDE);
      text_label_->SetFont(font_);
      SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kHorizontal, kHorizMargin, 0, kIconLabelSpacing));
      AddChildView(text_label_);
    }
    text_label_->SetText(animated_text);
    text_size_ = font_.GetStringWidth(animated_text);
    text_size_ += kHorizMargin;
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

void LocationBarDecorationView::OnPaintBackground(gfx::Canvas* canvas) {
  if (force_draw_text_ || (slide_animator_.get() &&
      (slide_animator_->is_animating() || pause_animation_)))
    background_painter_.Paint(canvas, size());
}

void LocationBarDecorationView::AnimationOnClick() {
  // Stop animation.
  if (slide_animator_.get() && slide_animator_->is_animating()) {
    PauseAnimation();
    slide_animator_->Reset();
  }
}
