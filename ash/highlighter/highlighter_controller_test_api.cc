// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller_test_api.h"

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_view.h"

namespace ash {

HighlighterControllerTestApi::HighlighterControllerTestApi(
    HighlighterController* instance)
    : instance_(instance) {
  instance_->SetObserver(this);
}

HighlighterControllerTestApi::~HighlighterControllerTestApi() {
  instance_->SetObserver(nullptr);
}

void HighlighterControllerTestApi::SetEnabled(bool enabled) {
  instance_->SetEnabled(enabled);
}

void HighlighterControllerTestApi::DestroyPointerView() {
  instance_->DestroyPointerView();
}

void HighlighterControllerTestApi::SimulateInterruptedStrokeTimeout() {
  if (!instance_->interrupted_stroke_timer_)
    return;
  instance_->interrupted_stroke_timer_->Stop();
  instance_->RecognizeGesture();
}

bool HighlighterControllerTestApi::IsShowingHighlighter() const {
  return instance_->highlighter_view_.get();
}

bool HighlighterControllerTestApi::IsFadingAway() const {
  return IsShowingHighlighter() && instance_->highlighter_view_->animating();
}

bool HighlighterControllerTestApi::IsShowingSelectionResult() const {
  return instance_->result_view_.get();
}

bool HighlighterControllerTestApi::IsWaitingToResumeStroke() const {
  return instance_->interrupted_stroke_timer_ &&
         instance_->interrupted_stroke_timer_->IsRunning();
}

const FastInkPoints& HighlighterControllerTestApi::points() const {
  return instance_->highlighter_view_->points_;
}

const FastInkPoints& HighlighterControllerTestApi::predicted_points() const {
  return instance_->highlighter_view_->predicted_points_;
}

void HighlighterControllerTestApi::HandleSelection(const gfx::Rect& rect) {
  handle_selection_called_ = true;
  selection_ = rect;
  // This is mimicking the logic implemented PaletteDelegateChromeOS,
  // which should eventually move to HighlighterController (crbug/761120).
  CallMetalayerDone();
}

void HighlighterControllerTestApi::HandleFailedSelection() {
  handle_failed_selection_called_ = true;
  // This is mimicking the logic implemented PaletteDelegateChromeOS,
  // which should eventually move to HighlighterController (crbug/761120).
  if (via_button_)
    CallMetalayerDone();
}

}  // namespace ash
