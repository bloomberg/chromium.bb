// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_coordinator.h"

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_coordinator.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Type of child coordinator presented by this coordinator.
typedef NS_ENUM(NSInteger, PresentedCoordinator) {
  PresentedCoordinatorUnifiedConsent,
  PresentedCoordinatorPersonalization,
};
}  // namespace

@interface ConsentBumpCoordinator ()<ConsentBumpViewControllerDelegate>

// Which child coordinator is currently presented.
@property(nonatomic, assign) PresentedCoordinator presentedCoordinator;

// The ViewController of this coordinator, redefined as a
// ConsentBumpViewController.
@property(nonatomic, strong)
    ConsentBumpViewController* consentBumpViewController;
// The child coordinator presenting the unified consent.
@property(nonatomic, strong)
    UnifiedConsentCoordinator* unifiedConsentCoordinator;
// The child coordinator presenting the presonalization content.
@property(nonatomic, strong)
    ConsentBumpPersonalizationCoordinator* personalizationCoordinator;

@end

@implementation ConsentBumpCoordinator

@synthesize presentedCoordinator = _presentedCoordinator;
@synthesize consentBumpViewController = _consentBumpViewController;
@synthesize unifiedConsentCoordinator = _unifiedConsentCoordinator;
@synthesize personalizationCoordinator = _personalizationCoordinator;

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.consentBumpViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.consentBumpViewController = [[ConsentBumpViewController alloc] init];
  self.consentBumpViewController.delegate = self;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc] init];
  [self.unifiedConsentCoordinator start];
  self.presentedCoordinator = PresentedCoordinatorUnifiedConsent;

  self.consentBumpViewController.contentViewController =
      self.unifiedConsentCoordinator.viewController;
}

- (void)stop {
  self.consentBumpViewController = nil;
  self.unifiedConsentCoordinator = nil;
  self.personalizationCoordinator = nil;
}

#pragma mark - ConsentBumpViewControllerDelegate

- (void)consentBumpViewControllerDidTapPrimaryButton:
    (ConsentBumpViewController*)consentBumpViewController {
  switch (self.presentedCoordinator) {
    case PresentedCoordinatorUnifiedConsent:
      // TODO(crbug.com/866506): Consent bump accepted.
      break;
    case PresentedCoordinatorPersonalization:
      // TODO(crbug.com/866506): Clarify what should be the behavior at this
      // point.
      break;
  }
}

- (void)consentBumpViewControllerDidTapSecondaryButton:
    (ConsentBumpViewController*)consentBumpViewController {
  switch (self.presentedCoordinator) {
    case PresentedCoordinatorUnifiedConsent:
      // Present the personlization.
      if (!self.personalizationCoordinator) {
        self.personalizationCoordinator =
            [[ConsentBumpPersonalizationCoordinator alloc]
                initWithBaseViewController:nil];
        [self.personalizationCoordinator start];
      }
      self.consentBumpViewController.contentViewController =
          self.personalizationCoordinator.viewController;
      self.presentedCoordinator = PresentedCoordinatorPersonalization;
      break;

    case PresentedCoordinatorPersonalization:
      // Go back to the unified consent.
      self.consentBumpViewController.contentViewController =
          self.unifiedConsentCoordinator.viewController;
      self.presentedCoordinator = PresentedCoordinatorUnifiedConsent;
      break;
  }
}

@end
