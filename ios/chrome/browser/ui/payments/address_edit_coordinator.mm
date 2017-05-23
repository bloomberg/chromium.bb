// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/logging.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillUITypeFromAutofillType;
using ::AutofillTypeFromAutofillUIType;
}  // namespace

@interface AddressEditCoordinator ()

@property(nonatomic, strong)
    CountrySelectionCoordinator* countrySelectionCoordinator;

@property(nonatomic, strong) AddressEditViewController* viewController;

@property(nonatomic, strong) AddressEditMediator* mediator;

@end

@implementation AddressEditCoordinator

@synthesize address = _address;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize countrySelectionCoordinator = _countrySelectionCoordinator;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.viewController = [[AddressEditViewController alloc] init];
  // TODO(crbug.com/602666): Title varies depending on what field is missing.
  // e.g., Add Email vs. Add Phone Number.
  NSString* title = self.address
                        ? l10n_util::GetNSString(IDS_PAYMENTS_EDIT_ADDRESS)
                        : l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS);
  [self.viewController setTitle:title];
  [self.viewController setDelegate:self];
  [self.viewController setValidatorDelegate:self];
  self.mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:self.paymentRequest
                                                  address:self.address];
  [self.mediator setConsumer:self.viewController];
  [self.viewController setDataSource:self.mediator];
  [self.viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [[self baseViewController].navigationController
      pushViewController:self.viewController
                animated:YES];
}

- (void)stop {
  [self.viewController.navigationController popViewControllerAnimated:YES];
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  // TODO(crbug.com/602666): Validation.
  return nil;
}

#pragma mark - AddressEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                          didSelectField:(EditorField*)field {
  if (field.autofillUIType == AutofillUITypeProfileHomeAddressCountry) {
    self.countrySelectionCoordinator = [[CountrySelectionCoordinator alloc]
        initWithBaseViewController:self.viewController];
    [self.countrySelectionCoordinator setCountries:self.mediator.countries];
    [self.countrySelectionCoordinator
        setSelectedCountryCode:self.mediator.selectedCountryCode];
    [self.countrySelectionCoordinator setDelegate:self];
    [self.countrySelectionCoordinator start];
  }
}

- (void)addressEditViewController:(AddressEditViewController*)controller
           didFinishEditingFields:(NSArray<EditorField*>*)fields {
  // TODO(crbug.com/602666): Create or edit an address as appropriate.
  [self.delegate addressEditCoordinator:self
                didFinishEditingAddress:self.address];
}

- (void)addressEditViewControllerDidCancel:
    (AddressEditViewController*)controller {
  [self.delegate addressEditCoordinatorDidCancel:self];
}

#pragma mark - CountrySelectionCoordinatorDelegate

- (void)countrySelectionCoordinator:(CountrySelectionCoordinator*)coordinator
           didSelectCountryWithCode:(NSString*)countryCode {
  if (self.mediator.selectedCountryCode != countryCode) {
    [self.mediator setSelectedCountryCode:countryCode];
    [self.viewController loadModel];
    [self.viewController.collectionView reloadData];
  }
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
}

@end
