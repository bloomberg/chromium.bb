// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_coordinator.h"

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
#import "ios/chrome/browser/ui/payments/contact_info_edit_mediator.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;
}  // namespace

@interface ContactInfoEditCoordinator ()

@property(nonatomic, strong) PaymentRequestEditViewController* viewController;

@property(nonatomic, strong) ContactInfoEditMediator* mediator;

@end

@implementation ContactInfoEditCoordinator

@synthesize profile = _profile;
@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.viewController = [[PaymentRequestEditViewController alloc] init];
  // TODO(crbug.com/602666): Title varies depending on what field is missing.
  // e.g., Add Email vs. Add Phone Number.
  NSString* title =
      self.profile
          ? l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CONTACT_DETAILS_LABEL)
          : l10n_util::GetNSString(IDS_PAYMENTS_ADD_CONTACT_DETAILS_LABEL);
  [self.viewController setTitle:title];
  [self.viewController setDelegate:self];
  [self.viewController setValidatorDelegate:self];
  self.mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:self.paymentRequest
                     profile:self.profile];
  [self.mediator setConsumer:self.viewController];
  [self.viewController setDataSource:self.mediator];
  [self.viewController loadModel];

  [[self baseViewController] presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[self.viewController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    switch (field.autofillUIType) {
      case AutofillUITypeProfileHomePhoneWholeNumber: {
        const std::string countryCode =
            autofill::AutofillCountry::CountryCodeForLocale(
                GetApplicationContext()->GetApplicationLocale());
        if (!autofill::IsValidPhoneNumber(base::SysNSStringToUTF16(field.value),
                                          countryCode)) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }
      case AutofillUITypeProfileEmailAddress: {
        if (!autofill::IsValidEmailAddress(
                base::SysNSStringToUTF16(field.value))) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_EMAIL_INVALID_VALIDATION_MESSAGE);
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
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  // Create an empty autofill profile. If a profile is being edited, copy over
  // the information.
  autofill::AutofillProfile profile =
      self.profile ? *self.profile
                   : autofill::AutofillProfile(base::GenerateGUID(),
                                               autofill::kSettingsOrigin);

  for (EditorField* field in fields) {
    profile.SetRawInfo(AutofillTypeFromAutofillUIType(field.autofillUIType),
                       base::SysNSStringToUTF16(field.value));
  }

  if (!self.profile) {
    self.paymentRequest->GetPersonalDataManager()->AddProfile(profile);

    // Add the profile to the list of profiles in |self.paymentRequest|.
    self.profile = self.paymentRequest->AddAutofillProfile(profile);
  } else {
    // Override the origin.
    profile.set_origin(autofill::kSettingsOrigin);
    self.paymentRequest->GetPersonalDataManager()->UpdateProfile(profile);

    // Cached profile must be invalidated once the profile is modified.
    _paymentRequest->profile_comparator()->Invalidate(profile);

    // Update the original profile instance that is being edited.
    *self.profile = profile;
  }

  [self.delegate contactInfoEditCoordinator:self
                    didFinishEditingProfile:self.profile];
}

- (void)paymentRequestEditViewControllerDidCancel:
    (PaymentRequestEditViewController*)controller {
  [self.delegate contactInfoEditCoordinatorDidCancel:self];
}

@end
