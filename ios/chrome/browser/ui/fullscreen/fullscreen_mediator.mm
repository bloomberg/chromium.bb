// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_to_top_animator.h"
#import "ios/chrome/browser/ui/fullscreen/toolbar_reveal_animator.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenMediator::FullscreenMediator(FullscreenController* controller,
                                       FullscreenModel* model)
    : controller_(controller), model_(model) {
  DCHECK(controller_);
  DCHECK(model_);
  model_->AddObserver(this);
}

FullscreenMediator::~FullscreenMediator() {
  // Disconnect() is expected to be called before deallocation.
  DCHECK(!controller_);
  DCHECK(!model_);
}

void FullscreenMediator::ScrollToTop() {
  DCHECK(!scroll_to_top_animator_);
  CGFloat progress = model_->progress();
  scroll_to_top_animator_ =
      [[FullscreenScrollToTopAnimator alloc] initWithStartProgress:progress];
  SetUpAnimator(&scroll_to_top_animator_);
  for (auto& observer : observers_) {
    observer.FullscreenWillScrollToTop(controller_, scroll_to_top_animator_);
  }
  StartAnimator(&scroll_to_top_animator_);
}

void FullscreenMediator::WillEnterForeground() {
  if (toolbar_reveal_animator_)
    return;
  CGFloat progress = model_->progress();
  toolbar_reveal_animator_ =
      [[ToolbarRevealAnimator alloc] initWithStartProgress:progress];
  SetUpAnimator(&toolbar_reveal_animator_);
  for (auto& observer : observers_) {
    observer.FullscreenWillEnterForeground(controller_,
                                           toolbar_reveal_animator_);
  }
  StartAnimator(&toolbar_reveal_animator_);
}

void FullscreenMediator::AnimateModelReset() {
  if (toolbar_reveal_animator_)
    return;
  CGFloat progress = model_->progress();
  toolbar_reveal_animator_ =
      [[ToolbarRevealAnimator alloc] initWithStartProgress:progress];
  SetUpAnimator(&toolbar_reveal_animator_);
  for (auto& observer : observers_) {
    observer.FullscreenModelWasReset(controller_, toolbar_reveal_animator_);
  }
  StartAnimator(&toolbar_reveal_animator_);
}

void FullscreenMediator::Disconnect() {
  [scroll_end_animator_ stopAnimation:YES];
  scroll_end_animator_ = nil;
  [scroll_to_top_animator_ stopAnimation:YES];
  scroll_to_top_animator_ = nil;
  [toolbar_reveal_animator_ stopAnimation:YES];
  toolbar_reveal_animator_ = nil;
  model_->RemoveObserver(this);
  model_ = nullptr;
  controller_ = nullptr;
}

void FullscreenMediator::FullscreenModelProgressUpdated(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }
}

void FullscreenMediator::FullscreenModelEnabledStateChanged(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
  for (auto& observer : observers_) {
    observer.FullscreenEnabledStateChanged(controller_, model->enabled());
  }
}

void FullscreenMediator::FullscreenModelScrollEventStarted(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
}

void FullscreenMediator::FullscreenModelScrollEventEnded(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  DCHECK(!scroll_end_animator_);
  CGFloat progress = model_->progress();
  if (AreCGFloatsEqual(progress, 0.0) || AreCGFloatsEqual(progress, 1.0))
    return;
  scroll_end_animator_ =
      [[FullscreenScrollEndAnimator alloc] initWithStartProgress:progress];
  SetUpAnimator(&scroll_end_animator_);
  for (auto& observer : observers_) {
    observer.FullscreenScrollEventEnded(controller_, scroll_end_animator_);
  }
  StartAnimator(&scroll_end_animator_);
}

void FullscreenMediator::FullscreenModelWasReset(FullscreenModel* model) {
  // Stop any in-progress animations.  Don't update the model because this
  // callback occurs after the model's state is reset, and updating the model
  // the with active animator's current value would overwrite the reset value.
  StopAnimating(false /* update_model */);
  // Update observers for the reset progress value.
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }
}

void FullscreenMediator::SetUpAnimator(__strong FullscreenAnimator** animator) {
  VerifyAnimatorPointer(animator);
  [*animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    DCHECK_EQ(finalPosition, UIViewAnimatingPositionEnd);
    if (!*animator)
      return;
    model_->AnimationEndedWithProgress(
        [*animator progressForAnimatingPosition:finalPosition]);
    *animator = nil;
  }];
}

void FullscreenMediator::StartAnimator(__strong FullscreenAnimator** animator) {
  // Start the animator if animations have been added to it, or reset the ivar
  // otherwise.
  VerifyAnimatorPointer(animator);
  if ((*animator).hasAnimations) {
    [*animator startAnimation];
  } else {
    *animator = nil;
  }
}

void FullscreenMediator::StopAnimating(bool update_model) {
  if (!scroll_end_animator_ && !scroll_to_top_animator_ &&
      !toolbar_reveal_animator_) {
    return;
  }

  // At most one animator should be non-nil.
  DCHECK_EQ((scroll_end_animator_ ? 1 : 0) + (scroll_to_top_animator_ ? 1 : 0) +
                (toolbar_reveal_animator_ ? 1 : 0),
            1);

  if (scroll_end_animator_)
    StopAnimator(&scroll_end_animator_, update_model);
  if (scroll_to_top_animator_)
    StopAnimator(&scroll_to_top_animator_, update_model);
  if (toolbar_reveal_animator_)
    StopAnimator(&toolbar_reveal_animator_, update_model);
}

void FullscreenMediator::StopAnimator(__strong FullscreenAnimator** animator,
                                      bool update_model) {
  VerifyAnimatorPointer(animator);
  DCHECK_EQ((*animator).state, UIViewAnimatingStateActive);
  if (update_model)
    model_->AnimationEndedWithProgress((*animator).currentProgress);
  [*animator stopAnimation:YES];
  *animator = nil;
}

void FullscreenMediator::VerifyAnimatorPointer(
    __strong FullscreenAnimator** animator) const {
  DCHECK(animator);
  DCHECK(*animator);
  DCHECK(*animator == scroll_end_animator_ ||
         *animator == scroll_to_top_animator_ ||
         *animator == toolbar_reveal_animator_);
}
