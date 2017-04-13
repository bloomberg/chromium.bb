// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_gesture_recognizer.h"

#import <UIKit/UIGestureRecognizerSubclass.h>

#import "base/ios/weak_nsobject.h"

@interface OverscrollActionsGestureRecognizer () {
  base::WeakNSProtocol<id> target_;
  SEL action_;
}
@end

@implementation OverscrollActionsGestureRecognizer

- (instancetype)initWithTarget:(id)target action:(SEL)action {
  self = [super initWithTarget:target action:action];
  if (self) {
    target_.reset([target retain]);
    action_ = action;
  }
  return self;
}

- (void)reset {
  [super reset];
  [target_ performSelector:action_ withObject:self];
}

- (void)removeTarget:(id)target action:(SEL)action {
  DCHECK(target != target_);
  [super removeTarget:target action:action];
}

@end
