// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()<UIViewControllerTransitioningDelegate>
// Object that internally backs the public TabSwitcher
@property(nonatomic, strong) TabGridAdaptor* adaptor;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize mainViewController = _mainViewController;
// Public properties.
@synthesize animationsDisabledForTesting = _animationsDisabledForTesting;
// Private properties.
@synthesize adaptor = _adaptor;
@synthesize bvcContainer = _bvcContainer;

#pragma mark - Public properties

- (id<TabSwitcher>)tabSwitcher {
  return self.adaptor;
}

#pragma mark - MainCoordinator properties

- (id<ViewControllerSwapping>)viewControllerSwapper {
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  UIViewController* mainViewController = [[TabGridViewController alloc] init];
  mainViewController.transitioningDelegate = self;
  _mainViewController = mainViewController;
  self.window.rootViewController = self.mainViewController;
  self.adaptor = [[TabGridAdaptor alloc] init];
  self.adaptor.tabGridViewController = self.mainViewController;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  return nil;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  // Verify that the presenting and dismissed view controllers are of the
  // expected types.
  return nil;
}

#pragma mark - ViewControllerSwapping

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    DCHECK_EQ(self.bvcContainer,
              self.mainViewController.presentedViewController);
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  }
  return self.mainViewController;
}

- (UIViewController*)viewController {
  return self.mainViewController;
}

- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher
             completion:(ProceduralBlock)completion {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.mainViewController);
  // It's also expected that |tabSwitcher| will be |self.tabSwitcher|, but that
  // may not be worth a DCHECK?

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    self.bvcContainer.transitioningDelegate = self;
    self.bvcContainer = nil;
    BOOL animated = !self.animationsDisabledForTesting;
    [self.mainViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    if (completion) {
      completion();
    }
  }
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController);

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.transitioningDelegate = self;
  BOOL animated = !self.animationsDisabledForTesting;
  [self.mainViewController presentViewController:self.bvcContainer
                                        animated:animated
                                      completion:completion];
}

@end
