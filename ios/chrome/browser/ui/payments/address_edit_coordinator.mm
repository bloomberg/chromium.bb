// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
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

@property(nonatomic, strong) UINavigationController* viewController;

@property(nonatomic, strong)
    PaymentRequestEditViewController* editViewController;

@property(nonatomic, strong) AddressEditMediator* mediator;

@end

@implementation AddressEditCoordinator

@synthesize address = _address;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize countrySelectionCoordinator = _countrySelectionCoordinator;
@synthesize viewController = _viewController;
@synthesize editViewController = _editViewController;
@synthesize mediator = _mediator;

- (void)start {
  self.editViewController = [[PaymentRequestEditViewController alloc] init];
  // TODO(crbug.com/602666): Title varies depending on what field is missing.
  // e.g., Add Email vs. Add Phone Number.
  NSString* title = self.address
                        ? l10n_util::GetNSString(IDS_PAYMENTS_EDIT_ADDRESS)
                        : l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS);
  [self.editViewController setTitle:title];
  [self.editViewController setDelegate:self];
  [self.editViewController setValidatorDelegate:self];
  self.mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:self.paymentRequest
                                                  address:self.address];
  [self.mediator setConsumer:self.editViewController];
  [self.editViewController setDataSource:self.mediator];
  [self.editViewController loadModel];

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:self.editViewController];
  [self.viewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.viewController
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  [self.viewController setNavigationBarHidden:YES];

  [[self baseViewController] presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[self.viewController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
  self.editViewController = nil;
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    switch (field.autofillUIType) {
      case AutofillUITypeProfileHomePhoneWholeNumber: {
        const std::string selectedCountryCode =
            base::SysNSStringToUTF8(self.mediator.selectedCountryCode);
        if (!autofill::IsValidPhoneNumber(base::SysNSStringToUTF16(field.value),
                                          selectedCountryCode)) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }
      default:
        break;
    }
  } else if (field.isRequired) {
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
        initWithBaseViewController:self.editViewController];
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

    // Cached profile must be invalidated once the profile is modified.
    _paymentRequest->profile_comparator()->Invalidate(address);

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
    [self.editViewController loadModel];
    [self.editViewController.collectionView reloadData];
  }
  [self.countrySelectionCoordinator stop];
  self.countrySelectionCoordinator = nil;
}

@end
