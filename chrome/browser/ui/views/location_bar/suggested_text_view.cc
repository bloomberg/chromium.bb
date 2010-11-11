// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/suggested_text_view.h"

#include "app/multi_animation.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"

// Amount of time to wait before starting the animation.
static const int kPauseTimeMS = 1000;

// Duration of the animation in which the colors change.
static const int kFadeInTimeMS = 300;

SuggestedTextView::SuggestedTextView(LocationBarView* location_bar)
    : location_bar_(location_bar) {
}

SuggestedTextView::~SuggestedTextView() {
}

void SuggestedTextView::StartAnimation() {
  if (!InstantController::IsEnabled(location_bar_->profile(),
                                    InstantController::PREDICTIVE_TYPE)) {
    return;
  }
  StopAnimation();

  animation_.reset(CreateAnimation());
  animation_->Start();
  UpdateBackgroundColor();
}

void SuggestedTextView::StopAnimation() {
  if (animation_.get()) {
    // Reset the delegate so that we don't attempt to commit in AnimationEnded.
    animation_->set_delegate(NULL);
    animation_.reset(NULL);
    SetColor(LocationBarView::GetColor(ToolbarModel::NONE,
                                       LocationBarView::DEEMPHASIZED_TEXT));
    SchedulePaint();
  }
}

void SuggestedTextView::PaintBackground(gfx::Canvas* canvas) {
  if (!animation_.get() || animation_->GetCurrentValue() == 0)
    return;

  // TODO(sky): these numbers need to come from the edit.
  canvas->FillRectInt(bg_color_, 0, 2, width(), height() - 5);
}

void SuggestedTextView::AnimationEnded(const Animation* animation) {
  location_bar_->OnCommitSuggestedText();
}

void SuggestedTextView::AnimationProgressed(const Animation* animation) {
  UpdateBackgroundColor();

  SkColor fg_color = LocationBarView::GetColor(
      ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT);
  SkColor sel_fg_color = LocationBarView::GetColor(
      ToolbarModel::NONE, LocationBarView::SELECTED_TEXT);
  SetColor(color_utils::AlphaBlend(
      sel_fg_color, fg_color,
      Tween::ValueBetween(animation->GetCurrentValue(), 0, 255)));

  SchedulePaint();
}

void SuggestedTextView::AnimationCanceled(const Animation* animation) {
}

Animation* SuggestedTextView::CreateAnimation() {
  MultiAnimation::Parts parts;
  parts.push_back(MultiAnimation::Part(kPauseTimeMS, Tween::ZERO));
  parts.push_back(MultiAnimation::Part(kFadeInTimeMS, Tween::EASE_IN));
  MultiAnimation* animation = new MultiAnimation(parts);
  animation->set_delegate(this);
  animation->set_continuous(false);
  return animation;
}

void SuggestedTextView::UpdateBackgroundColor() {
  if (!animation_.get()) {
    // Background only painted while animating.
    return;
  }

  double value = animation_->GetCurrentValue();
  SkColor bg_color = LocationBarView::GetColor(ToolbarModel::NONE,
                                               LocationBarView::BACKGROUND);
#if defined(OS_WIN)
  SkColor s_color = color_utils::GetSysSkColor(COLOR_HIGHLIGHT);
#else
  // TODO(sky): fix me.
  NOTIMPLEMENTED();
  SkColor s_color = SK_ColorLTGRAY;
#endif
  bg_color_ = color_utils::AlphaBlend(s_color, bg_color,
                                      Tween::ValueBetween(value, 0, 255));
}
