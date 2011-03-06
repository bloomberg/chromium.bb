// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/suggested_text_view.h"

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"

SuggestedTextView::SuggestedTextView(AutocompleteEditModel* edit_model)
    : edit_model_(edit_model),
      bg_color_(0) {
}

SuggestedTextView::~SuggestedTextView() {
}

void SuggestedTextView::StartAnimation() {
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

void SuggestedTextView::OnPaintBackground(gfx::Canvas* canvas) {
  if (!animation_.get() || animation_->GetCurrentValue() == 0)
    return;

  // TODO(sky): these numbers need to come from the edit.
  canvas->FillRectInt(bg_color_, 0, 2, width(), height() - 5);
}

void SuggestedTextView::AnimationEnded(const ui::Animation* animation) {
  edit_model_->CommitSuggestedText(false);
}

void SuggestedTextView::AnimationProgressed(const ui::Animation* animation) {
  UpdateBackgroundColor();

  SkColor fg_color = LocationBarView::GetColor(
      ToolbarModel::NONE, LocationBarView::DEEMPHASIZED_TEXT);
  SkColor sel_fg_color = LocationBarView::GetColor(
      ToolbarModel::NONE, LocationBarView::SELECTED_TEXT);
  SetColor(color_utils::AlphaBlend(
      sel_fg_color, fg_color,
      ui::Tween::ValueBetween(animation->GetCurrentValue(), 0, 255)));

  SchedulePaint();
}

void SuggestedTextView::AnimationCanceled(const ui::Animation* animation) {
}

ui::Animation* SuggestedTextView::CreateAnimation() {
  ui::MultiAnimation::Parts parts;
  parts.push_back(ui::MultiAnimation::Part(
      InstantController::kAutoCommitPauseTimeMS, ui::Tween::ZERO));
  parts.push_back(ui::MultiAnimation::Part(
      InstantController::kAutoCommitFadeInTimeMS, ui::Tween::EASE_IN));
  ui::MultiAnimation* animation = new ui::MultiAnimation(parts);
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
                                      ui::Tween::ValueBetween(value, 0, 255));
}
