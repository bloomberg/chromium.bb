// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payment_request_util {

NSString* NameLabelFromAutofillProfile(autofill::AutofillProfile* profile) {
  return base::SysUTF16ToNSString(
      profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                       GetApplicationContext()->GetApplicationLocale()));
}

NSString* AddressLabelFromAutofillProfile(autofill::AutofillProfile* profile) {
  // Name, company, and country are not included in the shipping address label.
  std::vector<autofill::ServerFieldType> label_fields;
  label_fields.push_back(autofill::ADDRESS_HOME_LINE1);
  label_fields.push_back(autofill::ADDRESS_HOME_LINE2);
  label_fields.push_back(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  label_fields.push_back(autofill::ADDRESS_HOME_CITY);
  label_fields.push_back(autofill::ADDRESS_HOME_STATE);
  label_fields.push_back(autofill::ADDRESS_HOME_ZIP);
  label_fields.push_back(autofill::ADDRESS_HOME_SORTING_CODE);

  return base::SysUTF16ToNSString(profile->ConstructInferredLabel(
      label_fields, label_fields.size(),
      GetApplicationContext()->GetApplicationLocale()));
}

NSString* PhoneNumberLabelFromAutofillProfile(
    autofill::AutofillProfile* profile) {
  return base::SysUTF16ToNSString(profile->GetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
      GetApplicationContext()->GetApplicationLocale()));
}

web::PaymentAddress PaymentAddressFromAutofillProfile(
    autofill::AutofillProfile* profile) {
  web::PaymentAddress address;
  address.country = profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY);
  address.address_line.push_back(
      profile->GetRawInfo(autofill::ADDRESS_HOME_LINE1));
  address.address_line.push_back(
      profile->GetRawInfo(autofill::ADDRESS_HOME_LINE2));
  address.address_line.push_back(
      profile->GetRawInfo(autofill::ADDRESS_HOME_LINE3));
  address.region = profile->GetRawInfo(autofill::ADDRESS_HOME_STATE);
  address.city = profile->GetRawInfo(autofill::ADDRESS_HOME_CITY);
  address.dependent_locality =
      profile->GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  address.postal_code = profile->GetRawInfo(autofill::ADDRESS_HOME_ZIP);
  address.sorting_code =
      profile->GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE);
  address.language_code = base::UTF8ToUTF16(profile->language_code());
  address.organization = profile->GetRawInfo(autofill::COMPANY_NAME);
  address.recipient =
      profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                       GetApplicationContext()->GetApplicationLocale());
  address.phone = profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);

  return address;
}

NSString* GetShippingSectionTitle(PaymentRequest* payment_request) {
  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_SUMMARY_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_SUMMARY_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_SUMMARY_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorTitle(PaymentRequest* payment_request) {
  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_ADDRESS_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_ADDRESS_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorInfoMessage(
    PaymentRequest* payment_request) {
  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_SHIPPING_ADDRESS_FOR_SHIPPING_METHODS);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_DELIVERY_ADDRESS_FOR_DELIVERY_METHODS);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_PICKUP_ADDRESS_FOR_PICKUP_METHODS);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorErrorMessage(
    PaymentRequest* payment_request) {
  if (!payment_request->payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request->payment_details().error);

  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_ADDRESS);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_ADDRESS);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingOptionSelectorTitle(PaymentRequest* payment_request) {
  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_OPTION_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_OPTION_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_OPTION_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingOptionSelectorErrorMessage(
    PaymentRequest* payment_request) {
  if (!payment_request->payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request->payment_details().error);

  switch (payment_request->payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_OPTION);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_OPTION);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_OPTION);
    default:
      NOTREACHED();
      return @"";
  }
}

}  // namespace payment_request_util
