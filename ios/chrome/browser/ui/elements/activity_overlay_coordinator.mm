// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"

#include "base/mac/objc_release_properties.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

@interface ActivityOverlayCoordinator ()

// View controller that displays an activity indicator.
@property(nonatomic, retain) UIViewController* activityOverlayViewController;
@end

@implementation ActivityOverlayCoordinator

@synthesize activityOverlayViewController = _activityOverlayViewController;

- (void)dealloc {
  base::mac::ReleaseProperties(self);
  [super dealloc];
}

- (void)start {
  if (self.activityOverlayViewController)
    return;
  self.activityOverlayViewController =
      [[[ActivityOverlayViewController alloc] initWithNibName:nil bundle:nil]
          autorelease];
  [self.baseViewController
      addChildViewController:self.activityOverlayViewController];
  [self.baseViewController.view
      addSubview:self.activityOverlayViewController.view];
  [self.activityOverlayViewController
      didMoveToParentViewController:self.baseViewController];
  AddSameConstraints(self.baseViewController.view,
                     self.activityOverlayViewController.view);
}

- (void)stop {
  if (!self.activityOverlayViewController)
    return;
  [self.activityOverlayViewController willMoveToParentViewController:nil];
  [self.activityOverlayViewController.view removeFromSuperview];
  [self.activityOverlayViewController removeFromParentViewController];
  self.activityOverlayViewController = nil;
}

@end
