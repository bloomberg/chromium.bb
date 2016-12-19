// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode/safe_mode_coordinator.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/app/safe_mode/safe_mode_view_controller.h"
#include "ios/chrome/browser/crash_loop_detection_util.h"

namespace {
const int kStartupCrashLoopThreshold = 2;
}

@interface SafeModeCoordinator ()<SafeModeViewControllerDelegate> {
  // Weak pointer to window passed on init.
  base::WeakNSObject<UIWindow> _window;
  // Weak pointer backing property of the same name.
  base::WeakNSProtocol<id<SafeModeCoordinatorDelegate>> _delegate;
}

@end

@implementation SafeModeCoordinator

#pragma mark - property implementation.

- (id<SafeModeCoordinatorDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<SafeModeCoordinatorDelegate>)delegate {
  _delegate.reset(delegate);
}

#pragma mark - Public class methods

+ (BOOL)shouldStart {
  // Check whether there appears to be a startup crash loop. If not, don't look
  // at anything else.
  if (crash_util::GetFailedStartupAttemptCount() < kStartupCrashLoopThreshold)
    return NO;

  return [SafeModeViewController hasSuggestions];
}

#pragma mark - ChromeCoordinator implementation

- (void)start {
  // Create the SafeModeViewController and make it the root view controller for
  // the window. The window has ownership of it and will dispose of it when
  // another view controller is made root.
  //
  // General note: Safe mode should be safe; it should not depend on other
  // objects being created. Be extremely conservative when adding code to this
  // method.
  base::scoped_nsobject<SafeModeViewController> viewController(
      [[SafeModeViewController alloc] initWithDelegate:self]);
  [self.window setRootViewController:viewController];

  // Reset the crash count; the user may change something based on the recovery
  // UI that will fix the crash, and having the next launch start in recovery
  // mode would be strange.
  crash_util::ResetFailedStartupAttemptCount();
}

// Override of ChildCoordinators method, which is not supported in this class.
- (MutableCoordinatorArray*)childCoordinators {
  NOTREACHED() << "Do not add child coordinators to SafeModeCoordinator.";
  return nil;
}

#pragma mark - SafeModeViewControllerDelegate implementation

- (void)startBrowserFromSafeMode {
  [self.delegate coordinatorDidExitSafeMode:self];
}

@end
