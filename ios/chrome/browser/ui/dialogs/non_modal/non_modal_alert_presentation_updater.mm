// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_presentation_updater.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_touch_forwarder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NonModalAlertPresentationUpdater ()
// The alert controller whose presentation is being controlled.
@property(nonatomic, weak, readonly) UIAlertController* alertController;
// The presentation container view for the alert.
@property(nonatomic, readonly) UIView* presentationContainerView;
@end

@implementation NonModalAlertPresentationUpdater

- (instancetype)initWithAlertController:(UIAlertController*)alertController {
  if (self = [super init]) {
    DCHECK(alertController);
    _alertController = alertController;
  }
  return self;
}

#pragma mark - Accessors

- (UIView*)presentationContainerView {
  return self.alertController.presentationController.containerView;
}

#pragma mark - Public

- (void)setUpNonModalPresentationWithViewportInsets:
    (UIEdgeInsets)viewportInsets {
  DCHECK(!self.presentationContainerView.layer.mask);

  // If the alert is being presented with animations, the presentation container
  // view will not be instantiated until the transition begins, so the mask is
  // set up in the transition's animation block.
  if (!self.presentationContainerView) {
    __weak NonModalAlertPresentationUpdater* weakSelf = self;
    auto addMaskBlock =
        ^void(id<UIViewControllerTransitionCoordinatorContext>) {
          [weakSelf setUpNonModalPresentationWithViewportInsets:viewportInsets];
        };
    [self.alertController.transitionCoordinator
        animateAlongsideTransition:addMaskBlock
                        completion:nil];
    return;
  }

  // Add the mask to the presentation container.
  CALayer* mask = [CALayer layer];
  mask.backgroundColor = [UIColor blackColor].CGColor;
  self.presentationContainerView.layer.mask = mask;
  [self updateMaskForViewportInsets:viewportInsets];

  // Add a touch forwarder for the mask.
  NonModalAlertTouchForwarder* touchForwarder =
      [[NonModalAlertTouchForwarder alloc] init];
  touchForwarder.forwardingTarget =
      self.alertController.presentingViewController.view;
  touchForwarder.mask = mask;
  UIView* alertView = self.alertController.view;
  [alertView.superview insertSubview:touchForwarder belowSubview:alertView];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // |-updateForFullscreenProgress:| is non-optional for performance reasons,
  // but the progress is never expected to change from 1.0 because fullscreen is
  // disabled.
  NOTREACHED() << "Fullscreen is disabled during non-modal alert presentation.";
}

- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
  [self updateMaskForViewportInsets:maxViewportInsets];
}

#pragma mark - Private

// Updates the alert's presentation container view mask using |viewportInsets|.
- (void)updateMaskForViewportInsets:(UIEdgeInsets)viewportInsets {
  self.presentationContainerView.layer.mask.frame = UIEdgeInsetsInsetRect(
      self.presentationContainerView.layer.bounds, viewportInsets);
}

@end
