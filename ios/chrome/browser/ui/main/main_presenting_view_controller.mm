// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_presenting_view_controller.h"

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MainPresentingViewController

- (UIViewController*)activeViewController {
  return self.presentedViewController;
}

- (void)showTabSwitcher:(UIViewController<TabSwitcher>*)tabSwitcher
             completion:(ProceduralBlock)completion {
  [self setActiveViewController:tabSwitcher completion:completion];
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  [self setActiveViewController:viewController completion:completion];
}

// Presents the given view controller, first dismissing any other view
// controllers that are currently presented.  Runs the given |completion| block
// after the view controller is visible.
- (void)setActiveViewController:(UIViewController*)activeViewController
                     completion:(void (^)())completion {
  DCHECK(activeViewController);
  if (self.activeViewController == activeViewController) {
    if (completion) {
      completion();
    }
    return;
  }

  // TODO(crbug.com/546189): DCHECK here that there isn't a modal view
  // controller showing once the known violations of that are fixed.

  if (self.activeViewController) {
    // This call must be to super, as the override of
    // dismissViewControllerAnimated:completion: below tries to dismiss using
    // self.activeViewController, but this call explicitly needs to present
    // using the MainPresentingViewController itself.
    [super dismissViewControllerAnimated:NO completion:nil];
  }

  // This call must be to super, as the override of
  // presentViewController:animated:completion: below tries to present using
  // self.activeViewController, but this call explicitly needs to present
  // using the MainPresentingViewController itself.
  [super presentViewController:activeViewController
                      animated:NO
                    completion:^{
                      DCHECK(self.activeViewController == activeViewController);
                      if (completion) {
                        completion();
                      }
                    }];
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

@end
