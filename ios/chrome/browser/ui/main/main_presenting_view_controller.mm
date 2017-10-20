// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_presenting_view_controller.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/main/transitions/bvc_container_to_tab_switcher_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BVCContainerViewController : UIViewController

@property(nonatomic, weak) UIViewController* currentBVC;

@end

@implementation BVCContainerViewController

- (UIViewController*)currentBVC {
  return [self.childViewControllers firstObject];
}

- (void)setCurrentBVC:(UIViewController*)bvc {
  DCHECK(bvc);
  if (self.currentBVC == bvc) {
    return;
  }

  // Remove the current bvc, if any.
  if (self.currentBVC) {
    [self.currentBVC willMoveToParentViewController:nil];
    [self.currentBVC.view removeFromSuperview];
    [self.currentBVC removeFromParentViewController];
  }

  DCHECK_EQ(nil, self.currentBVC);
  DCHECK_EQ(0U, self.view.subviews.count);

  // Add the new active view controller.
  [self addChildViewController:bvc];
  bvc.view.frame = self.view.bounds;
  [self.view addSubview:bvc.view];
  [bvc didMoveToParentViewController:self];

  // Let the system know that the child has changed so appearance updates can
  // be made.
  [self setNeedsStatusBarAppearanceUpdate];

  DCHECK(self.currentBVC == bvc);
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.currentBVC;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.currentBVC;
}

- (BOOL)shouldAutorotate {
  return self.currentBVC ? [self.currentBVC shouldAutorotate]
                         : [super shouldAutorotate];
}

@end

@interface MainPresentingViewController ()<
    UIViewControllerTransitioningDelegate>

@property(nonatomic, strong) BVCContainerViewController* bvcContainer;

// Redeclared as readwrite.
@property(nonatomic, readwrite, weak)
    UIViewController<TabSwitcher>* tabSwitcher;

@end

@implementation MainPresentingViewController
@synthesize tabSwitcher = _tabSwitcher;
@synthesize bvcContainer = _bvcContainer;

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    DCHECK_EQ(self.bvcContainer, self.presentedViewController);
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  } else if (self.tabSwitcher) {
    DCHECK_EQ(self.tabSwitcher, [self.childViewControllers firstObject]);
    return self.tabSwitcher;
  }

  return nil;
}

- (void)showTabSwitcher:(UIViewController<TabSwitcher>*)tabSwitcher
             completion:(ProceduralBlock)completion {
  DCHECK(tabSwitcher);

  // Don't remove and re-add the tabSwitcher if it hasn't changed.
  if (self.tabSwitcher != tabSwitcher) {
    // Remove any existing tab switchers first.
    if (self.tabSwitcher) {
      [self.tabSwitcher willMoveToParentViewController:nil];
      [self.tabSwitcher.view removeFromSuperview];
      [self.tabSwitcher removeFromParentViewController];
    }

    // Add the new tab switcher as a child VC.
    [self addChildViewController:tabSwitcher];
    tabSwitcher.view.frame = self.view.bounds;
    [self.view addSubview:tabSwitcher.view];
    [tabSwitcher didMoveToParentViewController:self];
    self.tabSwitcher = tabSwitcher;
  }

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    self.bvcContainer.transitioningDelegate = self;
    self.bvcContainer = nil;
    [super dismissViewControllerAnimated:YES completion:completion];
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
  [super presentViewController:self.bvcContainer
                      animated:(self.tabSwitcher != nil)
                    completion:completion];
}

#pragma mark - UIViewController methods

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  // If there is no activeViewController then this call will get inadvertently
  // dropped.
  DCHECK(self.activeViewController);
  [self.activeViewController presentViewController:viewControllerToPresent
                                          animated:flag
                                        completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  // If there is no activeViewController then this call will get inadvertently
  // dropped.
  DCHECK(self.activeViewController);
  [self.activeViewController dismissViewControllerAnimated:flag
                                                completion:completion];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.activeViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.activeViewController;
}

- (BOOL)shouldAutorotate {
  return self.activeViewController
             ? [self.activeViewController shouldAutorotate]
             : [super shouldAutorotate];
}

#pragma mark - Transitioning Delegate

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  // Verify that the presenting and dismissed view controllers are of the
  // expected types.
  DCHECK([dismissed isKindOfClass:[BVCContainerViewController class]]);
  DCHECK([dismissed.presentingViewController
      isKindOfClass:[MainPresentingViewController class]]);

  BVCContainerToTabSwitcherAnimator* animator =
      [[BVCContainerToTabSwitcherAnimator alloc] init];
  animator.tabSwitcher = self.tabSwitcher;
  return animator;
}

@end
