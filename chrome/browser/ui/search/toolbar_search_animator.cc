// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/toolbar_search_animator.h"

#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_types.h"
#include "chrome/browser/ui/search/toolbar_search_animator_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "ui/base/animation/multi_animation.h"

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
      background_change_delay_ms_(kBackgroundChangeDelayMs),
      background_change_duration_ms_(kBackgroundChangeDurationMs),
      is_omnibox_popup_open_(false) {
  search_model_->AddObserver(this);
}

ToolbarSearchAnimator::~ToolbarSearchAnimator() {
  if (background_animation_.get())
    background_animation_->Stop();
  search_model_->RemoveObserver(this);
}

double ToolbarSearchAnimator::GetGradientOpacity() const {
  if (background_animation_.get() && background_animation_->is_animating())
    return background_animation_->CurrentValueBetween(kMinOpacity, kMaxOpacity);
  return search_model_->mode().is_ntp() ? kMinOpacity : kMaxOpacity;
}

bool ToolbarSearchAnimator::IsToolbarSeparatorVisible() const {
  // The toolbar separator is only visible in 2 scenarios:
  // 1) when mode is |SEARCH_SUGGESTIONS| and the omnibox popup has finished
  //    retracting before the navigation URL was committed, i.e. before the
  //    mode was changed to |DEFAULT|.
  // 2) when mode is |DEFAULT| and the omnibox popup has finished retracting.
  return !is_omnibox_popup_open_ &&
      (search_model_->mode().mode == Mode::MODE_SEARCH_SUGGESTIONS ||
       search_model_->mode().is_default());
}

void ToolbarSearchAnimator::OnOmniboxPopupClosed() {
  is_omnibox_popup_open_ = false;
  FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                    OnToolbarSeparatorChanged());
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

void ToolbarSearchAnimator::ModeChanged(const Mode& old_mode,
                                        const Mode& new_mode) {
  bool prev_omnibox_popup_opened = is_omnibox_popup_open_;
  is_omnibox_popup_open_ = new_mode.mode == Mode::MODE_SEARCH_SUGGESTIONS;

  // If the mode transitions from |NTP| to |SEARCH| and we're not animating
  // background, start fading in gradient background.
  // TODO(kuan): check with UX folks if we need to animate from gradient to flat
  // when mode transitions from |SEARCH| or |DEFAULT| to |NTP|.
  if (new_mode.animate && old_mode.is_ntp() && new_mode.is_search() &&
      !(background_animation_.get() && background_animation_->is_animating())) {
    StartBackgroundChange();
    return;
  }

  // If the mode transitions from |SEARCH| to |DEFAULT| and we're already
  // animating background, let animation continue.
  if (new_mode.animate && old_mode.is_search() && new_mode.is_default() &&
      background_animation_.get() && background_animation_->is_animating()) {
    return;
  }

  // Notify observers if toolbar separator has changed.
  if (is_omnibox_popup_open_ != prev_omnibox_popup_opened) {
    FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                      OnToolbarSeparatorChanged());
  }

  // For all other cases, reset animation.
  // Stopping animation will jump to the end of it.
  Reset(NULL);
}

void ToolbarSearchAnimator::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK_EQ(background_animation_.get(), animation);
  FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                    OnToolbarBackgroundAnimatorProgressed());
}

void ToolbarSearchAnimator::AnimationEnded(const ui::Animation* animation) {
  // We only get this callback when |animation| has run its full course.
  // If animation was canceled (in Reset()), we won't get an AnimationEnded()
  // callback because we had cleared the animation's delegate in Reset().
  DCHECK_EQ(background_animation_.get(), animation);
  FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                    OnToolbarBackgroundAnimatorProgressed());
}

void ToolbarSearchAnimator::InitBackgroundAnimation() {
  ui::MultiAnimation::Parts parts;
  parts.push_back(ui::MultiAnimation::Part(
      background_change_delay_ms_ * InstantUI::GetSlowAnimationScaleFactor(),
      ui::Tween::ZERO));
  parts.push_back(ui::MultiAnimation::Part(
      background_change_duration_ms_ * InstantUI::GetSlowAnimationScaleFactor(),
      ui::Tween::LINEAR));
  background_animation_.reset(new ui::MultiAnimation(parts));
  background_animation_->set_continuous(false);
  background_animation_->set_delegate(this);
}

void ToolbarSearchAnimator::StartBackgroundChange() {
  InitBackgroundAnimation();
  background_animation_->Start();
}

void ToolbarSearchAnimator::Reset(TabContents* tab_contents) {
  bool notify_background_observers =
      background_animation_.get() && background_animation_->is_animating();

  background_animation_.reset();

  // Notify observers of animation cancelation.
  if (notify_background_observers) {
    FOR_EACH_OBSERVER(ToolbarSearchAnimatorObserver, observers_,
                      OnToolbarBackgroundAnimatorCanceled(tab_contents));
  }
}

}  // namespace search
}  // namespace chrome
