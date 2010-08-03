// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/menu_tracked_button.h"

@implementation MenuTrackedButton

- (void)mouseEntered:(NSEvent*)theEvent {
  didEnter_ = YES;
  [super mouseEntered:theEvent];
}

- (void)mouseExited:(NSEvent*)theEvent {
  didEnter_ = NO;
  tracking_ = NO;
  [super mouseExited:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  tracking_ = !didEnter_;
  [super mouseDragged:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent {
  if (!tracking_) {
    return [super mouseUp:theEvent];
  }
  [self performClick:self];
}

@end
