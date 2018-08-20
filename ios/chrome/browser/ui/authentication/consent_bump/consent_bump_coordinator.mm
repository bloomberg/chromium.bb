// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_coordinator.h"

#include "base/logging.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_mediator.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_coordinator.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using unified_consent::metrics::UnifiedConsentBumpAction;
using unified_consent::metrics::RecordConsentBumpMetric;

namespace {

void RecordMetricWithConsentBumpOptionType(ConsentBumpOptionType type) {
  UnifiedConsentBumpAction action;
  switch (type) {
    case ConsentBumpOptionTypeNotSet:
      NOTREACHED();
      action = UnifiedConsentBumpAction::kUnifiedConsentBumpActionDefaultOptIn;
      break;
    case ConsentBumpOptionTypeDefaultYesImIn:
      action = UnifiedConsentBumpAction::kUnifiedConsentBumpActionDefaultOptIn;
      break;
    case ConsentBumpOptionTypeMoreOptionsNoChange:
      action = UnifiedConsentBumpAction::
          kUnifiedConsentBumpActionMoreOptionsNoChanges;
      break;
    case ConsentBumpOptionTypeMoreOptionsReview:
      action = UnifiedConsentBumpAction::
          kUnifiedConsentBumpActionMoreOptionsReviewSettings;
      break;
    case ConsentBumpOptionTypeMoreOptionsTurnOn:
      action =
          UnifiedConsentBumpAction::kUnifiedConsentBumpActionMoreOptionsOptIn;
      break;
  }
  RecordConsentBumpMetric(action);
}

}  // namespace

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

+ (BOOL)shouldShowConsentBumpWithBrowserState:
    (ios::ChromeBrowserState*)browserState {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForBrowserState(browserState);
  return consent_service && consent_service->ShouldShowConsentBump();
}

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
  DCHECK(self.browserState);
  self.consentBumpViewController = [[ConsentBumpViewController alloc] init];
  self.consentBumpViewController.delegate = self;

  self.mediator = [[ConsentBumpMediator alloc] init];
  self.mediator.consumer = self.consentBumpViewController;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc] init];
  self.unifiedConsentCoordinator.delegate = self;
  self.unifiedConsentCoordinator.interactable = NO;
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
      type = ConsentBumpOptionTypeDefaultYesImIn;
      break;
    case ConsentBumpScreenPersonalization:
      type = self.personalizationCoordinator.selectedOption;
      DCHECK_NE(ConsentBumpOptionTypeDefaultYesImIn, type);
      break;
  }
  unified_consent::UnifiedConsentService* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForBrowserState(self.browserState);
  DCHECK(unifiedConsentService);
  switch (type) {
    case ConsentBumpOptionTypeDefaultYesImIn:
    case ConsentBumpOptionTypeMoreOptionsTurnOn:
    case ConsentBumpOptionTypeMoreOptionsReview:
      unifiedConsentService->SetUnifiedConsentGiven(true);
      break;
    case ConsentBumpOptionTypeMoreOptionsNoChange:
      break;
    case ConsentBumpOptionTypeNotSet:
      NOTREACHED();
      break;
  }
  RecordMetricWithConsentBumpOptionType(type);
  BOOL showSettings = type == ConsentBumpOptionTypeMoreOptionsReview;
  [self.delegate consentBumpCoordinator:self
         didFinishNeedingToShowSettings:showSettings];
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
