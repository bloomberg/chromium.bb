// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"

#import "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_view_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

@interface ActivityOverlayCoordinator () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_ActivityOverlayCoordinator;
}

// View controller that displays an activity indicator.
@property(nonatomic, retain) UIViewController* activityOverlayViewController;
@end

@implementation ActivityOverlayCoordinator

@synthesize activityOverlayViewController = _activityOverlayViewController;

- (nullable instancetype)initWithBaseViewController:
    (UIViewController*)viewController {
  self = [super initWithBaseViewController:viewController];
  if (self) {
    _propertyReleaser_ActivityOverlayCoordinator.Init(
        self, [ActivityOverlayCoordinator class]);
  }
  return self;
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
  AddSameCenterConstraints(self.baseViewController.view,
                           self.activityOverlayViewController.view);
  AddSameSizeConstraint(self.baseViewController.view,
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
