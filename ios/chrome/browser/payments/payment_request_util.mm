// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_util.h"

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/web/public/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payment_request_util {

NSString* GetNameLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                      GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetShippingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label = payments::GetShippingAddressLabelFormAutofillProfile(
      profile, GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetBillingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label = payments::GetBillingAddressLabelFromAutofillProfile(
      profile, GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetPhoneNumberLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label = payments::data_util::GetFormattedPhoneNumberForDisplay(
      profile, GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetEmailLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label =
      profile.GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                      GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetAddressNotificationLabelFromAutofillProfile(
    PaymentRequest& payment_request,
    const autofill::AutofillProfile& profile) {
  base::string16 label =
      payment_request.profile_comparator()->GetStringForMissingShippingFields(
          profile);
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetShippingSectionTitle(payments::PaymentShippingType shipping_type) {
  switch (shipping_type) {
    case payments::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_SUMMARY_LABEL);
    case payments::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_SUMMARY_LABEL);
    case payments::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_SUMMARY_LABEL);
    default:
      NOTREACHED();
      return nil;
  }
}

NSString* GetShippingAddressSelectorErrorMessage(
    const PaymentRequest& payment_request) {
  if (!payment_request.payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request.payment_details().error);

  switch (payment_request.shipping_type()) {
    case payments::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
    case payments::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_ADDRESS);
    case payments::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_ADDRESS);
    default:
      NOTREACHED();
      return nil;
  }
}

NSString* GetShippingOptionSelectorErrorMessage(
    const PaymentRequest& payment_request) {
  if (!payment_request.payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request.payment_details().error);

  switch (payment_request.shipping_type()) {
    case payments::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_OPTION);
    case payments::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_OPTION);
    case payments::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_OPTION);
    default:
      NOTREACHED();
      return nil;
  }
}

}  // namespace payment_request_util
