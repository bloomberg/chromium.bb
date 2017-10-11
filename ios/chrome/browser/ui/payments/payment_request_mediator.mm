// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/ui/payments/payment_request_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/strings_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// String used as the "URL" to take the user to the settings page for card and
// address options. Needs to be URL-like; otherwise, the link will not appear
// as a link in the UI (see setLabelLinkURL: in CollectionViewFooterCell).
const char kSettingsURL[] = "settings://card-and-address";

using ::payments::GetShippingOptionSectionString;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetShippingSectionTitle;
}  // namespace

@interface PaymentRequestMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

@end

@implementation PaymentRequestMediator

@synthesize totalValueChanged = _totalValueChanged;
@synthesize paymentRequest = _paymentRequest;

- (instancetype)initWithPaymentRequest:
    (payments::PaymentRequest*)paymentRequest {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
  }
  return self;
}

#pragma mark - PaymentRequestViewControllerDataSource

- (BOOL)canPay {
  return self.paymentRequest->selected_payment_method() != nullptr &&
         (self.paymentRequest->selected_shipping_option() != nullptr ||
          ![self requestShipping]) &&
         (self.paymentRequest->selected_shipping_profile() != nullptr ||
          ![self requestShipping]) &&
         (self.paymentRequest->selected_contact_profile() != nullptr ||
          ![self requestContactInfo]);
}

- (BOOL)canShip {
  return !self.paymentRequest->shipping_options().empty() &&
         self.paymentRequest->selected_shipping_profile() != nullptr;
}

- (BOOL)hasPaymentItems {
  return !self.paymentRequest->payment_details().display_items.empty();
}

- (BOOL)requestShipping {
  return self.paymentRequest->request_shipping();
}

- (BOOL)requestContactInfo {
  return self.paymentRequest->request_payer_name() ||
         self.paymentRequest->request_payer_email() ||
         self.paymentRequest->request_payer_phone();
}

- (CollectionViewItem*)paymentSummaryItem {
  PriceItem* item = [[PriceItem alloc] init];
  item.item = base::SysUTF8ToNSString(
      self.paymentRequest->payment_details().total->label);
  payments::CurrencyFormatter* currencyFormatter =
      self.paymentRequest->GetOrCreateCurrencyFormatter();
  item.price = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(currencyFormatter->formatted_currency_code()),
      currencyFormatter->Format(
          self.paymentRequest->payment_details().total->amount.value)));
  item.notification = self.totalValueChanged
                          ? l10n_util::GetNSString(IDS_PAYMENTS_UPDATED_LABEL)
                          : nil;
  self.totalValueChanged = NO;
  if ([self hasPaymentItems]) {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (PaymentsTextItem*)shippingSectionHeaderItem {
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text = GetShippingSectionTitle(self.paymentRequest->shipping_type());
  return item;
}

- (CollectionViewItem*)shippingAddressItem {
  const autofill::AutofillProfile* profile =
      self.paymentRequest->selected_shipping_profile();
  if (profile) {
    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    item.name = GetNameLabelFromAutofillProfile(*profile);
    item.address = GetShippingAddressLabelFromAutofillProfile(*profile);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  CollectionViewDetailItem* item = [[CollectionViewDetailItem alloc] init];
  item.text = base::SysUTF16ToNSString(
      GetShippingAddressSectionString(self.paymentRequest->shipping_type()));
  if (self.paymentRequest->shipping_profiles().empty()) {
    item.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else if (!profile) {
    item.detailText = [l10n_util::GetNSString(IDS_CHOOSE)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (CollectionViewItem*)shippingOptionItem {
  const payments::PaymentShippingOption* option =
      self.paymentRequest->selected_shipping_option();
  if (option) {
    PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
    item.text = base::SysUTF8ToNSString(option->label);
    payments::CurrencyFormatter* currencyFormatter =
        self.paymentRequest->GetOrCreateCurrencyFormatter();
    item.detailText = base::SysUTF16ToNSString(
        currencyFormatter->Format(option->amount.value));
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  CollectionViewDetailItem* item = [[CollectionViewDetailItem alloc] init];
  item.text = base::SysUTF16ToNSString(
      GetShippingOptionSectionString(self.paymentRequest->shipping_type()));

  if (!option) {
    item.detailText = [l10n_util::GetNSString(IDS_CHOOSE)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (PaymentsTextItem*)paymentMethodSectionHeaderItem {
  if (!self.paymentRequest->selected_payment_method())
    return nil;
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text =
      l10n_util::GetNSString(IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
  return item;
}

- (CollectionViewItem*)paymentMethodItem {
  payments::PaymentInstrument* paymentMethod =
      self.paymentRequest->selected_payment_method();
  if (paymentMethod) {
    PaymentMethodItem* item = [[PaymentMethodItem alloc] init];
    item.methodID = base::SysUTF16ToNSString(paymentMethod->GetLabel());
    item.methodDetail = base::SysUTF16ToNSString(paymentMethod->GetSublabel());

    switch (paymentMethod->type()) {
      case payments::PaymentInstrument::Type::AUTOFILL: {
        item.methodTypeIcon = NativeImage(paymentMethod->icon_resource_id());
        break;
      }
      case payments::PaymentInstrument::Type::NATIVE_MOBILE_APP: {
        payments::IOSPaymentInstrument* mobileApp =
            static_cast<payments::IOSPaymentInstrument*>(paymentMethod);
        item.methodTypeIcon = mobileApp->icon_image();
        break;
      }
      case payments::PaymentInstrument::Type::SERVICE_WORKER_APP: {
        NOTIMPLEMENTED();
        break;
      }
    }

    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  CollectionViewDetailItem* item = [[CollectionViewDetailItem alloc] init];
  item.text =
      l10n_util::GetNSString(IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
  if (self.paymentRequest->payment_methods().empty()) {
    item.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else if (!paymentMethod) {
    item.detailText = [l10n_util::GetNSString(IDS_CHOOSE)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (PaymentsTextItem*)contactInfoSectionHeaderItem {
  if (!self.paymentRequest->selected_contact_profile())
    return nil;
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text = l10n_util::GetNSString(IDS_PAYMENTS_CONTACT_DETAILS_LABEL);
  return item;
}

- (CollectionViewItem*)contactInfoItem {
  const autofill::AutofillProfile* profile =
      self.paymentRequest->selected_contact_profile();
  if (profile) {
    DCHECK(self.paymentRequest->request_payer_name() ||
           self.paymentRequest->request_payer_email() ||
           self.paymentRequest->request_payer_phone());

    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    if (self.paymentRequest->request_payer_name())
      item.name = GetNameLabelFromAutofillProfile(*profile);
    if (self.paymentRequest->request_payer_phone())
      item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
    if (self.paymentRequest->request_payer_email())
      item.email = GetEmailLabelFromAutofillProfile(*profile);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  CollectionViewDetailItem* item = [[CollectionViewDetailItem alloc] init];
  item.text = l10n_util::GetNSString(IDS_PAYMENTS_CONTACT_DETAILS_LABEL);
  if (self.paymentRequest->contact_profiles().empty()) {
    item.detailText = [l10n_util::GetNSString(IDS_ADD)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else if (!profile) {
    item.detailText = [l10n_util::GetNSString(IDS_CHOOSE)
        uppercaseStringWithLocale:[NSLocale currentLocale]];
  } else {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (CollectionViewFooterItem*)footerItem {
  CollectionViewFooterItem* item = [[CollectionViewFooterItem alloc] init];

  // If no transaction has been completed so far, choose which string to display
  // as a function of the profile's signed in state. Otherwise, always show the
  // same string.
  const bool firstTransactionCompleted =
      _paymentRequest->GetPrefService()->GetBoolean(
          payments::kPaymentsFirstTransactionCompleted);
  if (firstTransactionCompleted) {
    item.text = l10n_util::GetNSString(IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS);
  } else {
    const std::string email = _paymentRequest->GetAuthenticatedEmail();
    if (!email.empty()) {
      const std::string formattedString = l10n_util::GetStringFUTF8(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_IN,
          base::UTF8ToUTF16(email));
      item.text = base::SysUTF8ToNSString(formattedString);
    } else {
      item.text = l10n_util::GetNSString(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_OUT);
    }
  }
  item.linkURL = GURL(kSettingsURL);
  return item;
}

@end
