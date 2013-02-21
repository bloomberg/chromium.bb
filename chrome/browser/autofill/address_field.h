// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"

class AutofillField;
class AutofillScanner;

class AddressField : public FormField {
 public:
  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  enum AddressType {
    kGenericAddress = 0,
    kBillingAddress,
    kShippingAddress
  };

  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseOneLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseOneLineAddressBilling);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseOneLineAddressShipping);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseTwoLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseThreeLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCity);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseState);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseZip);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseStateAndZipOneLabel);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCountry);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseTwoLineAddressMissingLabel);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCompany);

  AddressField();

  static bool ParseCompany(AutofillScanner* scanner,
                           AddressField* address_field);
  static bool ParseAddressLines(AutofillScanner* scanner,
                                AddressField* address_field);
  static bool ParseCountry(AutofillScanner* scanner,
                           AddressField* address_field);
  static bool ParseZipCode(AutofillScanner* scanner,
                           AddressField* address_field);
  static bool ParseCity(AutofillScanner* scanner,
                        AddressField* address_field);
  static bool ParseState(AutofillScanner* scanner,
                         AddressField* address_field);

  // Looks for an address type in the given text, which the caller must
  // convert to lowercase.
  static AddressType AddressTypeFromText(const string16& text);

  // Tries to determine the billing/shipping type of this address.
  AddressType FindType() const;

  const AutofillField* company_;   // optional
  const AutofillField* address1_;
  const AutofillField* address2_;  // optional
  const AutofillField* city_;
  const AutofillField* state_;     // optional
  const AutofillField* zip_;
  const AutofillField* zip4_;      // optional ZIP+4; we don't fill this yet
  const AutofillField* country_;   // optional

  AddressType type_;

  DISALLOW_COPY_AND_ASSIGN(AddressField);
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
