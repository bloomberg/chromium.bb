// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_touch_forwarder.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NonModalAlertTouchForwarder

- (void)setForwardingTarget:(UIView*)forwardingTarget {
  if (_forwardingTarget == forwardingTarget)
    return;
  _forwardingTarget = forwardingTarget;
  DCHECK(!_forwardingTarget || [self canForwardToTarget]);
}

#pragma mark - UIView

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  // The touch forwarder should not be added as a descendant of the target.
  DCHECK(!_forwardingTarget || [self canForwardToTarget]);
}

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  if ([self shouldForwardTouchAtLocation:point]) {
    CGPoint forwardedPoint = [self.forwardingTarget convertPoint:point
                                                        fromView:self];
    return [self.forwardingTarget hitTest:forwardedPoint withEvent:event];
  } else {
    return [super hitTest:point withEvent:event];
  }
}

#pragma mark - Private

// Whether a touch in this view at |location| should be forwarded to the
// forwarding target.
- (BOOL)shouldForwardTouchAtLocation:(CGPoint)location {
  if (![self canForwardToTarget] || !self.mask)
    return NO;
  return !CGRectContainsPoint(
      self.mask.bounds, [self.mask convertPoint:location fromLayer:self.layer]);
}

// Touches are forwarded by overriding |-hitTest:withEvent:|, but if the
// forwarder is a descendant of the target, forwarding will create an infinite
// loop.
- (BOOL)canForwardToTarget {
  return self.forwardingTarget &&
         ![self isDescendantOfView:self.forwardingTarget];
}

@end
