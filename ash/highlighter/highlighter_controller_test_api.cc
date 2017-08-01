// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller_test_api.h"

#include "ash/fast_ink/fast_ink_points.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/highlighter/highlighter_selection_observer.h"
#include "ash/highlighter/highlighter_view.h"

namespace ash {

namespace {

class DummyHighlighterObserver : public HighlighterSelectionObserver {
 public:
  DummyHighlighterObserver() {}
  ~DummyHighlighterObserver() override {}

 private:
  void HandleSelection(const gfx::Rect& rect) override {}
};

}  // namespace

HighlighterControllerTestApi::HighlighterControllerTestApi(
    HighlighterController* instance)
    : instance_(instance),
      observer_(base::MakeUnique<DummyHighlighterObserver>()) {}

HighlighterControllerTestApi::~HighlighterControllerTestApi() {}

void HighlighterControllerTestApi::SetEnabled(bool enabled) {
  if (enabled)
    instance_->EnableHighlighter(observer_.get());
  else
    instance_->DisableHighlighter();
}

bool HighlighterControllerTestApi::IsShowingHighlighter() const {
  return instance_->highlighter_view_.get();
}

bool HighlighterControllerTestApi::IsFadingAway() const {
  return IsShowingHighlighter() &&
         instance_->highlighter_view_->animation_timer_.get();
}

bool HighlighterControllerTestApi::IsShowingSelectionResult() const {
  return instance_->result_view_.get();
}

const FastInkPoints& HighlighterControllerTestApi::points() const {
  return instance_->highlighter_view_->points_;
}

const FastInkPoints& HighlighterControllerTestApi::predicted_points() const {
  return instance_->highlighter_view_->predicted_points_;
}

}  // namespace ash
