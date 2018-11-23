// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"

#include <vector>

#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator ()<
    AutofillSecurityAlertPresenter,
    AddressCoordinatorDelegate,
    CardCoordinatorDelegate,
    ManualFillAccessoryViewControllerDelegate,
    PasswordCoordinatorDelegate,
    PasswordFetcherDelegate,
    PersonalDataManagerObserver> {
  // Personal data manager to be observed.
  autofill::PersonalDataManager* _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;
}

// The Mediator for the input accessory view controller.
@property(nonatomic, strong)
    FormInputAccessoryMediator* formInputAccessoryMediator;

// The View Controller for the input accessory view.
@property(nonatomic, strong)
    FormInputAccessoryViewController* formInputAccessoryViewController;

// The manual fill accessory to show above the keyboard.
@property(nonatomic, strong)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong)
    ManualFillInjectionHandler* manualFillInjectionHandler;

// The password fetcher used to inform if passwords are available.
@property(nonatomic, strong) PasswordFetcher* passwordFetcher;

// The WebStateList for this instance. Used to instantiate the child
// coordinators lazily.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation FormInputAccessoryCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  DCHECK(browserState);
  DCHECK(webStateList);
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;

    _manualFillInjectionHandler =
        [[ManualFillInjectionHandler alloc] initWithWebStateList:webStateList
                                          securityAlertPresenter:self];

    _formInputAccessoryViewController =
        [[FormInputAccessoryViewController alloc] init];

    if (autofill::features::IsPasswordManualFallbackEnabled()) {
      _manualFillAccessoryViewController =
          [[ManualFillAccessoryViewController alloc] initWithDelegate:self];
      _formInputAccessoryViewController.manualFillAccessoryViewController =
          _manualFillAccessoryViewController;
    }

    _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
        initWithConsumer:self.formInputAccessoryViewController
            webStateList:webStateList];

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);
    // In BVC unit tests the password store doesn't exist. Skip creating the
    // fetcher.
    // TODO:(crbug.com/878388) Remove this workaround.
    if (passwordStore) {
      _passwordFetcher =
          [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                                delegate:self];
    }
    autofill::PersonalDataManager* personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
    // There is no personal data manager in OTR (incognito).
    // TODO:(crbug.com/905720) Support Incognito.
    if (personalDataManager) {
      _personalDataManager = personalDataManager;
      _personalDataManagerObserver.reset(
          new autofill::PersonalDataManagerObserverBridge(self));
      personalDataManager->AddObserver(_personalDataManagerObserver.get());

      // TODO:(crbug.com/845472) Add earl grey test to verify the credit card
      // button is hidden when local cards are saved and then
      // kAutofillCreditCardEnabled is changed to disabled.
      _manualFillAccessoryViewController.creditCardButtonHidden =
          personalDataManager->GetCreditCards().empty();

      _manualFillAccessoryViewController.addressButtonHidden =
          personalDataManager->GetProfilesToSuggest().empty();
    } else {
      _manualFillAccessoryViewController.creditCardButtonHidden = YES;
      _manualFillAccessoryViewController.addressButtonHidden = YES;
    }
  }
  return self;
}

- (void)dealloc {
  if (_personalDataManager) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
  }
}

- (void)stop {
  [self stopChildren];
  [self.manualFillAccessoryViewController reset];
  [self.formInputAccessoryViewController restoreOriginalKeyboardView];
}

#pragma mark - Presenting Children

- (void)stopChildren {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

- (void)startPasswordsFromButton:(UIButton*)button {
  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                        browserState:self.browserState
                        webStateList:self.webStateList
                    injectionHandler:self.manualFillInjectionHandler];
  passwordCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [passwordCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:passwordCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:passwordCoordinator];
}

- (void)startCardsFromButton:(UIButton*)button {
  CardCoordinator* cardCoordinator = [[CardCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                    browserState:self.browserState
                    webStateList:self.webStateList
                injectionHandler:self.manualFillInjectionHandler];
  cardCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [cardCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:cardCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:cardCoordinator];
}

- (void)startAddressFromButton:(UIButton*)button {
  AddressCoordinator* addressCoordinator = [[AddressCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                    browserState:self.browserState
                injectionHandler:self.manualFillInjectionHandler];
  addressCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [addressCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:addressCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:addressCoordinator];
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self stopChildren];
  [self.formInputAccessoryMediator enableSuggestions];
  [self.formInputAccessoryViewController unlockManualFallbackView];
}

- (void)accountButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startAddressFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)cardButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startCardsFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startPasswordsFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordSettings {
  [self.delegate openPasswordSettings];
}

- (void)resetAccessoryView {
  [self.manualFillAccessoryViewController reset];
}

#pragma mark - CardCoordinatorDelegate

- (void)openCardSettings {
  [self.delegate openCreditCardSettings];
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressSettings {
  [self.delegate openAddressSettings];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>&)passwords {
  self.manualFillAccessoryViewController.passwordButtonHidden =
      passwords.empty();
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browserState);
  DCHECK(personalDataManager);

  self.manualFillAccessoryViewController.creditCardButtonHidden =
      personalDataManager->GetCreditCards().empty();

  self.manualFillAccessoryViewController.addressButtonHidden =
      personalDataManager->GetProfilesToSuggest().empty();
}

#pragma mark - AutofillSecurityAlertPresenter

- (void)presentSecurityWarningAlertWithText:(NSString*)body {
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
  NSString* defaltActionTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_OK_BUTTON);

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:alertTitle
                                          message:body
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:defaltActionTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];
  [alert addAction:defaultAction];
  [self.baseViewController presentViewController:alert
                                        animated:YES
                                      completion:nil];
}

@end
