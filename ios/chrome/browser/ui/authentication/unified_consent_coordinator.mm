// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/unified_consent_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/unified_consent_mediator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnifiedConsentCoordinator ()<UnifiedConsentViewControllerDelegate>

@property(nonatomic, strong) UnifiedConsentMediator* unifiedConsentMediator;
@property(nonatomic, strong, readwrite)
    UnifiedConsentViewController* unifiedConsentViewController;

@end

@implementation UnifiedConsentCoordinator

@synthesize delegate = _delegate;
@synthesize unifiedConsentMediator = _unifiedConsentMediator;
@synthesize unifiedConsentViewController = _unifiedConsentViewController;

- (void)start {
  self.unifiedConsentViewController =
      [[UnifiedConsentViewController alloc] init];
  self.unifiedConsentViewController.delegate = self;
  self.unifiedConsentMediator = [[UnifiedConsentMediator alloc]
      initWithUnifiedConsentViewController:self.unifiedConsentViewController];
  [self.unifiedConsentMediator start];
}

- (ChromeIdentity*)selectedIdentity {
  return self.unifiedConsentMediator.selectedIdentity;
}

- (UIViewController*)viewController {
  return self.unifiedConsentViewController;
}

- (int)openSettingsStringId {
  return self.unifiedConsentViewController.openSettingsStringId;
}

- (const std::vector<int>&)consentStringIds {
  return [self.unifiedConsentViewController consentStringIds];
}

#pragma mark - UnifiedConsentViewControllerDelegate

- (void)unifiedConsentViewControllerDidTapSettingsLink:
    (UnifiedConsentViewController*)controller {
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  [self.delegate unifiedConsentCoordinatorDidTapSettingsLink:self];
}

- (void)unifiedConsentViewControllerDidTapIdentityPickerView:
    (UnifiedConsentViewController*)controller {
  DCHECK_EQ(self.unifiedConsentViewController, controller);
  // TODO(crbug.com/827072): Needs implementation.
}

@end
