// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_mediator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Coordinator states.
typedef NS_ENUM(NSInteger, IdentityChooserCoordinatorState) {
  // Initiale state.
  IdentityChooserCoordinatorStateNotStarted = 0,
  // State when the view controller is displayed.
  IdentityChooserCoordinatorStateStarted,
  // State when the view is closed by tapping on "Add Account…" button.
  IdentityChooserCoordinatorStateClosedByAddingAccount,
  // State when the view is closed by selecting an identity.
  IdentityChooserCoordinatorStateClosedBySelectingIdentity,
  // State when the view is dismissed by tapping outside of the view.
  IdentityChooserCoordinatorStateClosedByDismiss,
};

}  // namespace

@interface IdentityChooserCoordinator ()<
    IdentityChooserViewControllerPresentationDelegate>

// Mediator.
@property(nonatomic, strong) IdentityChooserMediator* identityChooserMediator;
// View controller.
@property(nonatomic, strong)
    IdentityChooserViewController* identityChooserViewController;
// Coordinator state.
@property(nonatomic, assign) IdentityChooserCoordinatorState state;

@end

@implementation IdentityChooserCoordinator

@synthesize delegate = _delegate;
@synthesize identityChooserMediator = _identityChooserMediator;
@synthesize identityChooserViewController = _identityChooserViewController;
@synthesize state = _state;

- (void)start {
  [super start];
  DCHECK_EQ(IdentityChooserCoordinatorStateNotStarted, self.state);
  self.state = IdentityChooserCoordinatorStateStarted;
  // Creates the controller.
  self.identityChooserViewController =
      [[IdentityChooserViewController alloc] init];
  // Creates the mediator.
  self.identityChooserMediator = [[IdentityChooserMediator alloc] init];
  self.identityChooserMediator.identityChooserViewController =
      self.identityChooserViewController;
  // Setups.
  self.identityChooserViewController.presentationDelegate = self;
  // Starts.
  [self.identityChooserMediator start];
  [self.baseViewController
      presentViewController:self.identityChooserViewController
                   animated:YES
                 completion:nil];
}

#pragma mark - Setters/Getters

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  self.identityChooserMediator.selectedIdentity = selectedIdentity;
}

- (ChromeIdentity*)selectedIdentity {
  return self.identityChooserMediator.selectedIdentity;
}

#pragma mark - IdentityChooserViewControllerPresentationDelegate

- (void)identityChooserViewControllerDidDisappear:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  switch (self.state) {
    case IdentityChooserCoordinatorStateNotStarted:
    case IdentityChooserCoordinatorStateClosedByDismiss:
      NOTREACHED();
      break;
    case IdentityChooserCoordinatorStateStarted:
      self.state = IdentityChooserCoordinatorStateClosedByDismiss;
      break;
    case IdentityChooserCoordinatorStateClosedByAddingAccount:
      [self.delegate identityChooserCoordinatorDidTapOnAddAccount:self];
      break;
    case IdentityChooserCoordinatorStateClosedBySelectingIdentity:
      [self.delegate identityChooserCoordinator:self
                              didSelectIdentity:self.selectedIdentity];
      break;
  }
  [self.delegate identityChooserCoordinatorDidClose:self];
}

- (void)identityChooserViewControllerDidTapOnAddAccount:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  DCHECK_EQ(IdentityChooserCoordinatorStateStarted, self.state);
  self.state = IdentityChooserCoordinatorStateClosedByAddingAccount;
  [self.identityChooserViewController dismissViewControllerAnimated:YES
                                                         completion:nil];
}

- (void)identityChooserViewController:
            (IdentityChooserViewController*)viewController
          didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  DCHECK_EQ(IdentityChooserCoordinatorStateStarted, self.state);
  [self.identityChooserMediator selectIdentityWithGaiaID:gaiaID];
  self.state = IdentityChooserCoordinatorStateClosedBySelectingIdentity;
  [self.identityChooserViewController dismissViewControllerAnimated:YES
                                                         completion:nil];
}

@end
