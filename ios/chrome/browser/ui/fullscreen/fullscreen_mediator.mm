// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_scroll_end_animator.h"
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

void FullscreenMediator::Disconnect() {
  [animator_ stopAnimation:YES];
  animator_ = nil;
  model_->RemoveObserver(this);
  model_ = nullptr;
  controller_ = nullptr;
}

void FullscreenMediator::FullscreenModelProgressUpdated(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating();
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }
}

void FullscreenMediator::FullscreenModelEnabledStateChanged(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating();
  for (auto& observer : observers_) {
    observer.FullscreenEnabledStateChanged(controller_, model->enabled());
  }
}

void FullscreenMediator::FullscreenModelScrollEventStarted(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating();
}

void FullscreenMediator::FullscreenModelScrollEventEnded(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  DCHECK(!animator_);
  CGFloat progress = model_->progress();
  if (AreCGFloatsEqual(progress, 0.0) || AreCGFloatsEqual(progress, 1.0))
    return;
  animator_ =
      [[FullscreenScrollEndAnimator alloc] initWithStartProgress:progress];
  [animator_ addCompletion:^(UIViewAnimatingPosition finalPosition) {
    DCHECK_EQ(finalPosition, UIViewAnimatingPositionEnd);
    model_->AnimationEndedWithProgress(
        [animator_ progressForAnimatingPosition:finalPosition]);
    animator_ = nil;
  }];
  for (auto& observer : observers_) {
    observer.FullscreenScrollEventEnded(controller_, animator_);
  }
  [animator_ startAnimation];
}

void FullscreenMediator::StopAnimating() {
  if (!animator_)
    return;

  DCHECK_EQ(animator_.state, UIViewAnimatingStateActive);
  model_->AnimationEndedWithProgress(animator_.currentProgress);
  [animator_ stopAnimation:YES];
  animator_ = nil;
}
