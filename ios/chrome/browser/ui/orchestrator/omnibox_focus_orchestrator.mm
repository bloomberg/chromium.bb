// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/orchestrator/omnibox_focus_orchestrator.h"

#import "ios/chrome/browser/ui/orchestrator/toolbar_animatee.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxFocusOrchestrator

@synthesize toolbarAnimatee = _toolbarAnimatee;

- (void)transitionToStateOmniboxFocused:(BOOL)omniboxFocused
                        toolbarExpanded:(BOOL)toolbarExpanded
                               animated:(BOOL)animated {
  // TODO(crbug.com/805485): manage the changes in the location bar.
  if (toolbarExpanded) {
    [self updateUIToExpandedState:animated];
  } else {
    [self updateUIToContractedState:animated];
  }
}

#pragma mark - Private

// Updates the UI elements reflect the toolbar expanded state, |animated| or
// not.
- (void)updateUIToExpandedState:(BOOL)animated {
  void (^expansion)() = ^{
    [self.toolbarAnimatee expandLocationBar];
    [self.toolbarAnimatee showCancelButton];
  };

  void (^hideControls)() = ^{
    [self.toolbarAnimatee hideControlButtons];
  };

  if (animated) {
    // Use UIView animatieWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    [UIView animateWithDuration:ios::material::kDuration1
                          delay:0
                        options:UIViewAnimationCurveEaseInOut
                     animations:expansion
                     completion:nil];

    [UIView animateWithDuration:ios::material::kDuration2
                          delay:0
                        options:UIViewAnimationCurveEaseInOut
                     animations:hideControls
                     completion:nil];
  } else {
    expansion();
    hideControls();
  }
}

// Updates the UI elements reflect the toolbar contracted state, |animated| or
// not.
- (void)updateUIToContractedState:(BOOL)animated {
  void (^contraction)() = ^{
    [self.toolbarAnimatee contractLocationBar];
  };

  void (^hideCancel)() = ^{
    [self.toolbarAnimatee hideCancelButton];
  };

  void (^showControls)() = ^{
    [self.toolbarAnimatee showControlButtons];
  };

  if (animated) {
    // Use UIView animatieWithDuration instead of UIViewPropertyAnimator to
    // avoid UIKit bug. See https://crbug.com/856155.
    CGFloat totalDuration =
        ios::material::kDuration1 + ios::material::kDuration2;
    CGFloat relativeDurationAnimation1 =
        ios::material::kDuration1 / totalDuration;
    [UIView animateKeyframesWithDuration:totalDuration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          [UIView addKeyframeWithRelativeStartTime:0
                                  relativeDuration:relativeDurationAnimation1
                                        animations:^{
                                          contraction();
                                        }];
          [UIView
              addKeyframeWithRelativeStartTime:relativeDurationAnimation1
                              relativeDuration:1 - relativeDurationAnimation1
                                    animations:^{
                                      showControls();
                                    }];
        }
        completion:^(BOOL finished) {
          hideCancel();
        }];
  } else {
    contraction();
    showControls();
    hideCancel();
  }
}

@end
