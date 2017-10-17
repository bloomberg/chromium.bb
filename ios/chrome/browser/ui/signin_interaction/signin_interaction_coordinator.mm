// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"

#import "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_presenting.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninInteractionCoordinator ()<SigninInteractionPresenting>

// Coordinator to present alerts.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The BrowserState for this coordinator.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The controller managed by this coordinator.
@property(nonatomic, strong) SigninInteractionController* controller;

// The dispatcher to which commands should be sent.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// The UIViewController upon which UI should be presented.
@property(nonatomic, strong) UIViewController* presentingViewController;

@end

@implementation SigninInteractionCoordinator
@synthesize alertCoordinator = _alertCoordinator;
@synthesize browserState = _browserState;
@synthesize controller = _controller;
@synthesize dispatcher = _dispatcher;
@synthesize presentingViewController = _presentingViewController;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super init];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)signInWithIdentity:(ChromeIdentity*)identity
                 accessPoint:(signin_metrics::AccessPoint)accessPoint
                 promoAction:(signin_metrics::PromoAction)promoAction
    presentingViewController:(UIViewController*)viewController
                  completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController];

  [self.controller
      signInWithIdentity:identity
              completion:[self callbackToClearStateWithCompletion:completion]];
}

- (void)reAuthenticateWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
             presentingViewController:(UIViewController*)viewController
                           completion:
                               (signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController];

  [self.controller reAuthenticateWithCompletion:
                       [self callbackToClearStateWithCompletion:completion]];
}

- (void)addAccountWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
         presentingViewController:(UIViewController*)viewController
                       completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController];

  [self.controller addAccountWithCompletion:
                       [self callbackToClearStateWithCompletion:completion]];
}

- (void)cancel {
  [self.controller cancel];
}

- (void)cancelAndDismiss {
  [self.controller cancelAndDismiss];
}

- (BOOL)isActive {
  return self.controller != nil;
}

#pragma mark - SigninInteractionPresenting

- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  [self.presentingViewController presentViewController:viewController
                                              animated:animated
                                            completion:completion];
}

- (void)presentTopViewController:(UIViewController*)viewController
                        animated:(BOOL)animated
                      completion:(ProceduralBlock)completion {
  // TODO(crbug.com/754642): Stop using TopPresentedViewControllerFrom().
  UIViewController* topController =
      top_view_controller::TopPresentedViewControllerFrom(
          self.presentingViewController);
  [topController presentViewController:viewController
                              animated:animated
                            completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(ProceduralBlock)completion {
  [self.presentingViewController dismissViewControllerAnimated:animated
                                                    completion:completion];
}

- (void)presentError:(NSError*)error
       dismissAction:(ProceduralBlock)dismissAction {
  DCHECK(!self.alertCoordinator);
  // TODO(crbug.com/754642): Stop using TopPresentedViewControllerFrom().
  self.alertCoordinator =
      ErrorCoordinator(error, dismissAction,
                       top_view_controller::TopPresentedViewControllerFrom(
                           self.presentingViewController));
  [self.alertCoordinator start];
}

- (void)dismissError {
  [self.alertCoordinator executeCancelHandler];
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (BOOL)isPresenting {
  return self.presentingViewController.presentedViewController != nil;
}

#pragma mark - Private Methods

// Sets up relevant instance variables for a sign in operation.
- (void)
setupForSigninOperationWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                           promoAction:(signin_metrics::PromoAction)promoAction
              presentingViewController:
                  (UIViewController*)presentingViewController {
  self.presentingViewController = presentingViewController;

  self.controller = [[SigninInteractionController alloc]
      initWithBrowserState:self.browserState
      presentationProvider:self
               accessPoint:accessPoint
               promoAction:promoAction
                dispatcher:self.dispatcher];
}

// Returns a callback that clears the state of the coordinator and runs
// |completion|.
- (signin_ui::CompletionCallback)callbackToClearStateWithCompletion:
    (signin_ui::CompletionCallback)completion {
  __weak SigninInteractionCoordinator* weakSelf = self;
  signin_ui::CompletionCallback completionCallback = ^(BOOL success) {
    weakSelf.controller = nil;
    weakSelf.presentingViewController = nil;
    weakSelf.alertCoordinator = nil;
    if (completion) {
      completion(success);
    }
  };
  return completionCallback;
}

@end
