// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/captive_portal/captive_portal_login_coordinator.h"

#import "ios/chrome/browser/ui/captive_portal/captive_portal_login_view_controller.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CaptivePortalLoginCoordinator ()<
    CaptivePortalLoginViewControllerDelegate> {
  GURL _landingURL;
}

@property(nonatomic, strong, readonly)
    UINavigationController* loginNavigationController;

@end

@implementation CaptivePortalLoginCoordinator

@synthesize loginNavigationController = _loginNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                landingURL:(const GURL&)landingURL {
  self = [super initWithBaseViewController:viewController];
  if (self) {
    _landingURL = landingURL;
  }
  return self;
}

- (void)start {
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the login view controller.
  if (![self.baseViewController.view window] &&
      ![self.baseViewController.view isKindOfClass:[UIWindow class]]) {
    return;
  }

  [self.baseViewController presentViewController:self.loginNavigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [[_loginNavigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - Property Implementation.

- (UIViewController*)loginNavigationController {
  if (!_loginNavigationController) {
    // TODO(crbug.com/750129): Add delegate or callback block to
    // CaptivePortalLoginViewController in order to continue page load with
    // |callback_| if the user successfully connected to the captive portal.
    CaptivePortalLoginViewController* loginController =
        [[CaptivePortalLoginViewController alloc]
            initWithLandingURL:_landingURL];
    loginController.delegate = self;

    _loginNavigationController = [[UINavigationController alloc]
        initWithRootViewController:loginController];
    [_loginNavigationController
        setModalPresentationStyle:UIModalPresentationFormSheet];
    [_loginNavigationController
        setModalTransitionStyle:UIModalTransitionStyleCoverVertical];

    [_loginNavigationController setNavigationBarHidden:YES];
  }
  return _loginNavigationController;
}

#pragma mark - CaptivePortalLoginViewControllerDelegate

- (void)captivePortalLoginViewControllerDidFinish:
    (CaptivePortalLoginViewController*)controller {
  [self stop];
}

@end
