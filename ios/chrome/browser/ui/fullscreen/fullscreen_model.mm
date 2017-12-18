// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

#include <algorithm>

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Object that increments |counter| by 1 for its lifetime.
class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(size_t* counter) : counter_(counter) {
    ++(*counter_);
  }
  ~ScopedIncrementer() { --(*counter_); }

 private:
  size_t* counter_;
};
}

FullscreenModel::FullscreenModel() = default;
FullscreenModel::~FullscreenModel() = default;

void FullscreenModel::IncrementDisabledCounter() {
  if (++disabled_counter_ == 1U) {
    ScopedIncrementer disabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
  }
}

void FullscreenModel::DecrementDisabledCounter() {
  DCHECK_GT(disabled_counter_, 0U);
  if (!--disabled_counter_) {
    ScopedIncrementer enabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
  }
}

void FullscreenModel::ResetForNavigation() {
  SetProgress(1.0);
  scrolling_ = false;
  base_offset_ = NAN;
}

void FullscreenModel::AnimationEndedWithProgress(CGFloat progress) {
  DCHECK_GE(progress, 0.0);
  DCHECK_LE(progress, 1.0);
  // Since this is being set by the animator instead of by scroll events, do not
  // broadcast the new progress value.
  progress_ = progress;
}

void FullscreenModel::SetToolbarHeight(CGFloat toolbar_height) {
  DCHECK_GE(toolbar_height, 0.0);
  toolbar_height_ = toolbar_height;
  ResetForNavigation();
}

CGFloat FullscreenModel::GetToolbarHeight() const {
  return toolbar_height_;
}

void FullscreenModel::SetYContentOffset(CGFloat y_content_offset) {
  if (!enabled())
    return;

  y_content_offset_ = y_content_offset;

  if (!has_base_offset())
    UpdateBaseOffset();

  if (scrolling_ && !observer_callback_count_) {
    CGFloat delta = base_offset_ - y_content_offset_;
    SetProgress(1.0 + delta / toolbar_height_);
  } else {
    UpdateBaseOffset();
  }
}

CGFloat FullscreenModel::GetYContentOffset() const {
  return y_content_offset_;
}

void FullscreenModel::SetScrollViewIsScrolling(bool scrolling) {
  if (scrolling_ == scrolling)
    return;
  scrolling_ = scrolling;
  if (!scrolling_) {
    ScopedIncrementer scroll_ended_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventEnded(this);
    }
  }
}

bool FullscreenModel::ISScrollViewScrolling() const {
  return scrolling_;
}

void FullscreenModel::SetScrollViewIsDragging(bool dragging) {
  if (dragging_ == dragging)
    return;
  dragging_ = dragging;
  if (dragging_) {
    ScopedIncrementer scroll_started_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventStarted(this);
    }
    UpdateBaseOffset();
  }
}

bool FullscreenModel::IsScrollViewDragging() const {
  return dragging_;
}

void FullscreenModel::SetProgress(CGFloat progress) {
  progress = std::min(static_cast<CGFloat>(1.0), progress);
  progress = std::max(static_cast<CGFloat>(0.0), progress);
  if (AreCGFloatsEqual(progress_, progress))
    return;
  progress_ = progress;

  ScopedIncrementer progress_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelProgressUpdated(this);
  }
}

void FullscreenModel::UpdateBaseOffset() {
  base_offset_ = y_content_offset_ - (1.0 - progress_) * toolbar_height_;
}

void FullscreenModel::OnContentScrollOffsetBroadcasted(CGFloat offset) {
  SetYContentOffset(offset);
}

void FullscreenModel::OnScrollViewIsScrollingBroadcasted(bool scrolling) {
  SetScrollViewIsScrolling(scrolling);
}

void FullscreenModel::OnScrollViewIsDraggingBroadcasted(bool dragging) {
  SetScrollViewIsDragging(dragging);
}

void FullscreenModel::OnToolbarHeightBroadcasted(CGFloat toolbar_height) {
  SetToolbarHeight(toolbar_height);
}
