// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
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

@property(nonatomic, strong) PaymentRequestEditViewController* viewController;

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
  self.viewController = [[PaymentRequestEditViewController alloc] init];
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
  if (!field.value.length && field.isRequired) {
    return l10n_util::GetNSString(
        IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return nil;
}

#pragma mark - PaymentRequestEditViewControllerDelegate

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

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  // Create an empty autofill profile. If an address is being edited, copy over
  // the information.
  autofill::AutofillProfile address =
      self.address ? *self.address
                   : autofill::AutofillProfile(base::GenerateGUID(),
                                               autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    address.SetRawInfo(AutofillTypeFromAutofillUIType(field.autofillUIType),
                       base::SysNSStringToUTF16(field.value));
  }

  if (!self.address) {
    self.paymentRequest->GetPersonalDataManager()->AddProfile(address);

    // Add the profile to the list of profiles in |self.paymentRequest|.
    self.address = self.paymentRequest->AddAutofillProfile(address);
  } else {
    // Override the origin.
    address.set_origin(autofill::kSettingsOrigin);
    self.paymentRequest->GetPersonalDataManager()->UpdateProfile(address);

    // Update the original profile instance that is being edited.
    *self.address = address;
  }

  [self.delegate addressEditCoordinator:self
                didFinishEditingAddress:self.address];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
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
