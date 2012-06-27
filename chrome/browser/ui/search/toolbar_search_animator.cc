// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/toolbar_search_animator.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/search/toolbar_search_animator_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "ui/base/animation/slide_animation.h"

namespace {

const int kBackgroundChangeDelayMs = 100;
const int kBackgroundChangeDurationMs = 200;

const double kMinOpacity = 0.0f;
const double kMaxOpacity = 1.0f;

}  // namespace

namespace chrome {
namespace search {

ToolbarSearchAnimator::ToolbarSearchAnimator(SearchModel* search_model)
    : search_model_(search_model),
      animate_state_(ANIMATE_STATE_NONE) {
  search_model_->AddObserver(this);
}

ToolbarSearchAnimator::~ToolbarSearchAnimator() {
  search_model_->RemoveObserver(this);
}

void ToolbarSearchAnimator::GetCurrentBackgroundState(
    BackgroundState* background_state,
    double* search_background_opacity) const {
  // Should only be called for SEARCH mode.
  DCHECK(search_model_->mode().is_search());
  *background_state = BACKGROUND_STATE_DEFAULT;
  *search_background_opacity = -1.0f;
  switch (animate_state_) {
    case ANIMATE_STATE_WAITING:
      *background_state = BACKGROUND_STATE_NTP;
      break;
    case ANIMATE_STATE_RUNNING:
      *background_state = BACKGROUND_STATE_NTP_SEARCH;
      *search_background_opacity = background_animation_->CurrentValueBetween(
          kMinOpacity, kMaxOpacity);
      break;
    case ANIMATE_STATE_NONE:
      break;
  }
}

void ToolbarSearchAnimator::FinishAnimation(TabContents* tab_contents) {
  Reset(tab_contents);
}

void ToolbarSearchAnimator::AddObserver(
    ToolbarSearchAnimatorObserver* observer) {
  observers_.AddObserver(observer);
}

void ToolbarSearchAnimator::RemoveObserver(
    ToolbarSearchAnimatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ToolbarSearchAnimator::ModeChanged(const Mode& mode) {
  if (mode.is_search() && mode.animate &&
      animate_state_ == ANIMATE_STATE_NONE) {
    background_change_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBackgroundChangeDelayMs),
        this,
        &ToolbarSearchAnimator::StartBackgroundChange);
    animate_state_ = ANIMATE_STATE_WAITING;
    return;
  }
  // For all other cases, reset |animate_state_| and stop timer or animation.
  // Stopping animation will jump to the end of it.
  Reset(NULL);
}

void ToolbarSearchAnimator::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK_EQ(animation, background_animation_.get());
  animate_state_ = ANIMATE_STATE_RUNNING;
  FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                    OnToolbarBackgroundAnimatorProgressed());
}

void ToolbarSearchAnimator::AnimationEnded(const ui::Animation* animation) {
  DCHECK_EQ(animation, background_animation_.get());
  // Only notify observers via OnToolbarBackgroundAnimatorProgressed if the
  // animation has runs its full course i.e |animate_state_| is still
  // ANIMATE_STATE_RUNNING.
  // Animation that is canceled, i.e. |animate_state_| has been set to
  // ANIMATE_STATE_NONE in Reset, should notify observers via
  // OnToolbarBackgroundAnimatorCanceled.
  if (animate_state_ == ANIMATE_STATE_RUNNING) {
    animate_state_ = ANIMATE_STATE_NONE;
    FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                      OnToolbarBackgroundAnimatorProgressed());
  }
}

void ToolbarSearchAnimator::StartBackgroundChange() {
  if (!background_animation_.get()) {
    background_animation_.reset(new ui::SlideAnimation(this));
    background_animation_->SetTweenType(ui::Tween::LINEAR);
    background_animation_->SetSlideDuration(kBackgroundChangeDurationMs);
  }
  background_animation_->Reset(0.0);
  background_animation_->Show();
}

void ToolbarSearchAnimator::Reset(TabContents* tab_contents) {
  bool notify_observers = animate_state_ != ANIMATE_STATE_NONE;
  animate_state_ = ANIMATE_STATE_NONE;
  background_change_timer_.Stop();
  // If animation is still running, stopping it will trigger AnimationEnded
  // where we've prevented from notifying observers via BackgroundChanged;
  // see comments in AnimationEnded.
  if (background_animation_.get())
    background_animation_->Stop();
  if (notify_observers) {
    FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                      OnToolbarBackgroundAnimatorCanceled(tab_contents));
  }
}

}  // namespace search
}  // namespace chrome
