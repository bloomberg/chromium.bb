// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "ios/chrome/browser/application_context.h"

namespace payment_request_utils {

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

NSString* FormattedCurrencyString(NSDecimalNumber* value,
                                  NSString* currencyCode) {
  NSNumberFormatter* currencyFormatter =
      [[[NSNumberFormatter alloc] init] autorelease];
  [currencyFormatter setNumberStyle:NSNumberFormatterCurrencyStyle];
  [currencyFormatter setCurrencyCode:currencyCode];
  return [currencyFormatter stringFromNumber:value];
}

NSString* PhoneNumberLabelFromAutofillProfile(
    autofill::AutofillProfile* profile) {
  return base::SysUTF16ToNSString(profile->GetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
      GetApplicationContext()->GetApplicationLocale()));
}

}  // namespace payment_request_utils
