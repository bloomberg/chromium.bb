// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_FIELD_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_field.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

class AddressField : public FormField {
 public:
  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(ServerFieldTypeMap* map) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseOneLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseTwoLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseThreeLineAddress);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseStreetAddressFromTextArea);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCity);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseState);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseZip);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseStateAndZipOneLabel);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCountry);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseTwoLineAddressMissingLabel);
  FRIEND_TEST_ALL_PREFIXES(AddressFieldTest, ParseCompany);

  AddressField();

  bool ParseCompany(AutofillScanner* scanner);
  bool ParseAddressLines(AutofillScanner* scanner);
  bool ParseCountry(AutofillScanner* scanner);
  bool ParseZipCode(AutofillScanner* scanner);
  bool ParseCity(AutofillScanner* scanner);
  bool ParseState(AutofillScanner* scanner);

  const AutofillField* company_;
  const AutofillField* address1_;
  const AutofillField* address2_;
  const AutofillField* street_address_;
  const AutofillField* city_;
  const AutofillField* state_;
  const AutofillField* zip_;
  const AutofillField* zip4_;  // optional ZIP+4; we don't fill this yet.
  const AutofillField* country_;

  DISALLOW_COPY_AND_ASSIGN(AddressField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_FIELD_H_
