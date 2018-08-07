// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_mediator.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_coordinator.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsentBumpCoordinator ()<ConsentBumpViewControllerDelegate,
                                     UnifiedConsentCoordinatorDelegate>

// Which child coordinator is currently presented.
@property(nonatomic, assign) ConsentBumpScreen presentedCoordinatorType;

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
// Mediator for this coordinator.
@property(nonatomic, strong) ConsentBumpMediator* mediator;

@end

@implementation ConsentBumpCoordinator

@synthesize delegate = _delegate;
@synthesize presentedCoordinatorType = _presentedCoordinatorType;
@synthesize consentBumpViewController = _consentBumpViewController;
@synthesize unifiedConsentCoordinator = _unifiedConsentCoordinator;
@synthesize personalizationCoordinator = _personalizationCoordinator;
@synthesize mediator = _mediator;

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.consentBumpViewController;
}

- (void)setPresentedCoordinatorType:
    (ConsentBumpScreen)presentedCoordinatorType {
  _presentedCoordinatorType = presentedCoordinatorType;
  [self.mediator updateConsumerForConsentBumpScreen:presentedCoordinatorType];
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.consentBumpViewController = [[ConsentBumpViewController alloc] init];
  self.consentBumpViewController.delegate = self;

  self.mediator = [[ConsentBumpMediator alloc] init];
  self.mediator.consumer = self.consentBumpViewController;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc] init];
  self.unifiedConsentCoordinator.delegate = self;
  [self.unifiedConsentCoordinator start];
  self.presentedCoordinatorType = ConsentBumpScreenUnifiedConsent;

  self.consentBumpViewController.contentViewController =
      self.unifiedConsentCoordinator.viewController;
}

- (void)stop {
  self.mediator = nil;
  self.consentBumpViewController = nil;
  self.unifiedConsentCoordinator = nil;
  self.personalizationCoordinator = nil;
}

#pragma mark - ConsentBumpViewControllerDelegate

- (void)consentBumpViewControllerDidTapPrimaryButton:
    (ConsentBumpViewController*)consentBumpViewController {
  ConsentBumpOptionType type = ConsentBumpOptionTypeNotSet;
  switch (self.presentedCoordinatorType) {
    case ConsentBumpScreenUnifiedConsent:
      type = ConsentBumpOptionTypeTurnOn;
      break;
    case ConsentBumpScreenPersonalization:
      type = self.personalizationCoordinator.selectedOption;
      break;
  }
  switch (type) {
    case ConsentBumpOptionTypeNoChange:
      // TODO(crbug.com/866506): Implement metrics.
      break;
    case ConsentBumpOptionTypeReview:
      // TODO(crbug.com/866506): Implement metrics.
      break;
    case ConsentBumpOptionTypeTurnOn:
      // TODO(crbug.com/866506): Implement metrics + sync updates.
      break;
    case ConsentBumpOptionTypeNotSet:
      NOTREACHED();
      break;
  }
  [self.delegate consentBumpCoordinator:self
         didFinishNeedingToShowSettings:(type == ConsentBumpOptionTypeReview)];
}

- (void)consentBumpViewControllerDidTapSecondaryButton:
    (ConsentBumpViewController*)consentBumpViewController {
  switch (self.presentedCoordinatorType) {
    case ConsentBumpScreenUnifiedConsent:
      // Present the personlization.
      if (!self.personalizationCoordinator) {
        self.personalizationCoordinator =
            [[ConsentBumpPersonalizationCoordinator alloc]
                initWithBaseViewController:nil];
        [self.personalizationCoordinator start];
      }
      self.consentBumpViewController.contentViewController =
          self.personalizationCoordinator.viewController;
      self.presentedCoordinatorType = ConsentBumpScreenPersonalization;
      break;

    case ConsentBumpScreenPersonalization:
      // Go back to the unified consent.
      self.consentBumpViewController.contentViewController =
          self.unifiedConsentCoordinator.viewController;
      self.presentedCoordinatorType = ConsentBumpScreenUnifiedConsent;
      break;
  }
}

- (void)consentBumpViewControllerDidTapMoreButton:
    (ConsentBumpViewController*)consentBumpViewController {
  [self.unifiedConsentCoordinator scrollToBottom];
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator {
  [self.mediator consumerCanProceed];
}

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  NOTREACHED();
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  NOTREACHED();
}

@end
