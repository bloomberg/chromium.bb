// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/root/root_container_view_controller.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation RootContainerViewController
@synthesize contentViewController = _contentViewController;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor blackColor];
  [self attachChildViewController:self.contentViewController];
}

#pragma mark - Public properties

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (self.contentViewController == contentViewController)
    return;
  if ([self isViewLoaded]) {
    [self detachChildViewController:self.contentViewController];
    [self attachChildViewController:contentViewController];
  }
  _contentViewController = contentViewController;
}

#pragma mark - ChildViewController helper methods

- (void)attachChildViewController:(UIViewController*)viewController {
  if (!viewController) {
    return;
  }
  [self addChildViewController:viewController];
  viewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  viewController.view.frame = self.view.bounds;
  [self.view addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
}

- (void)detachChildViewController:(UIViewController*)viewController {
  if (!viewController) {
    return;
  }
  DCHECK_EQ(self, viewController.parentViewController);
  [viewController willMoveToParentViewController:nil];
  [viewController.view removeFromSuperview];
  [viewController removeFromParentViewController];
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  if ([self.contentViewController
          conformsToProtocol:@protocol(ZoomTransitionDelegate)]) {
    return [static_cast<id<ZoomTransitionDelegate>>(self.contentViewController)
        rectForZoomWithKey:key
                    inView:view];
  }
  return CGRectNull;
}

#pragma mark - MenuPresentationDelegate

- (CGRect)frameForMenuPresentation:(UIPresentationController*)presentation {
  CGSize menuSize = presentation.presentedView.frame.size;
  CGRect menuRect;
  menuRect.size = menuSize;

  CGRect menuOriginRect = [self rectForZoomWithKey:nil inView:self.view];
  if (CGRectIsNull(menuOriginRect)) {
    menuRect.origin = CGPointMake(50, 50);
    return menuRect;
  }
  // Calculate which corner of the menu the origin rect is in. This is
  // determined by comparing frames, and thus is RTL-independent.
  if (CGRectGetMinX(menuOriginRect) - CGRectGetMinX(self.view.bounds) <
      CGRectGetMaxX(self.view.bounds) - CGRectGetMaxX(menuOriginRect)) {
    // Origin rect is closer to the left edge of |self.view| than to the right.
    menuRect.origin.x = CGRectGetMinX(menuOriginRect);
  } else {
    // Origin rect is closer to the right edge of |self.view| than to the left.
    menuRect.origin.x = CGRectGetMaxX(menuOriginRect) - menuSize.width;
  }

  if (CGRectGetMinY(menuOriginRect) - CGRectGetMinY(self.view.bounds) <
      CGRectGetMaxY(self.view.bounds) - CGRectGetMaxY(menuOriginRect)) {
    // Origin rect is closer to the top edge of |self.view| than to the bottom.
    menuRect.origin.y = CGRectGetMinY(menuOriginRect);
  } else {
    // Origin rect is closer to the bottom edge of |self.view| than to the top.
    menuRect.origin.y = CGRectGetMaxY(menuOriginRect) - menuSize.height;
  }

  return menuRect;
}

@end
