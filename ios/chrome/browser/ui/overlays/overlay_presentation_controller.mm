// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OverlayPresentationController

#pragma mark - Accessors

- (BOOL)resizesPresentationContainer {
  return NO;
}

#pragma mark - UIPresentationController

- (BOOL)shouldPresentInFullscreen {
  return NO;
}

- (void)containerViewWillLayoutSubviews {
  [super containerViewWillLayoutSubviews];
  // Trigger a layout pass for the presenting view controller.  This allows the
  // presentation context to resize itself to match the presented overlay UI if
  // |resizesPresentationContainer| is YES.
  [self.presentingViewController.view setNeedsLayout];
}

@end
