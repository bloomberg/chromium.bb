// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/keyboard/hardware_keyboard_watcher.h"

#import <CoreGraphics/CoreGraphics.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Whether firstRect has a non null rect intersection with secondRect, yet does
// not fully include it.
bool IntersectsButDoesNotInclude(CGRect firstRect, CGRect secondRect) {
  return CGRectIntersectsRect(firstRect, secondRect) &&
         !CGRectContainsRect(firstRect, secondRect);
}

}  // namespace

@interface HardwareKeyboardWatcher () {
  base::scoped_nsobject<UIView> _accessoryView;
}
@end

@implementation HardwareKeyboardWatcher

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithAccessoryView:(UIView*)accessoryView {
  DCHECK(accessoryView);
  self = [super init];
  if (self) {
    _accessoryView.reset(accessoryView);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillChangeFrame:)
               name:UIKeyboardWillChangeFrameNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  // Don't handle keyboard notifications not involving the set accessory view.
  if ([_accessoryView window] == nil)
    return;

  NSDictionary* userInfo = [notification userInfo];
  CGRect beginKeyboardFrame =
      [userInfo[UIKeyboardFrameBeginUserInfoKey] CGRectValue];
  CGRect endKeyboardFrame =
      [userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  CGRect screenBounds = [UIScreen mainScreen].bounds;

  // During rotations, the reported keyboard frames are in the screen
  // coordinates *prior* to the rotation, while the screen already has its
  // new coordinates. http://crbug.com/511267
  // To alleviate that, switch the screen bounds width and height if needed.
  if (CGRectGetHeight(screenBounds) == CGRectGetWidth(beginKeyboardFrame)) {
    screenBounds.size =
        CGSizeMake(CGRectGetHeight(screenBounds), CGRectGetWidth(screenBounds));
  }

  // CGRectZero frames are seen when moving a split dock. Split keyboard means
  // software keyboard.
  bool hasCGRectZeroFrames =
      CGRectEqualToRect(CGRectZero, beginKeyboardFrame) ||
      CGRectEqualToRect(CGRectZero, endKeyboardFrame);

  bool keyboardIsPartiallyOnScreen =
      IntersectsButDoesNotInclude(screenBounds, beginKeyboardFrame) ||
      IntersectsButDoesNotInclude(screenBounds, endKeyboardFrame);

  bool isInHarwareKeyboardMode =
      !hasCGRectZeroFrames && keyboardIsPartiallyOnScreen;

  UMA_HISTOGRAM_BOOLEAN("Omnibox.HardwareKeyboardModeEnabled",
                        isInHarwareKeyboardMode);
}

@end
