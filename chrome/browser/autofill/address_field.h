// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_field.h"

class AutofillField;
class AutofillScanner;

class AddressField : public FormField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const OVERRIDE;

  static AddressField* Parse(AutofillScanner* scanner, bool is_ecml);

  // Tries to determine the billing/shipping type of this address.
  AddressType FindType() const;

  // Returns true if this is a full address as opposed to an address fragment
  // such as a stand-alone ZIP code.
  bool IsFullAddress();

 private:
  AddressField();

  static bool ParseCompany(AutofillScanner* scanner,
                           bool is_ecml,
                           AddressField* address_field);
  static bool ParseAddressLines(AutofillScanner* scanner,
                                bool is_ecml,
                                AddressField* address_field);
  static bool ParseCountry(AutofillScanner* scanner,
                           bool is_ecml,
                           AddressField* address_field);
  static bool ParseZipCode(AutofillScanner* scanner,
                           bool is_ecml,
                           AddressField* address_field);
  static bool ParseCity(AutofillScanner* scanner,
                        bool is_ecml,
                        AddressField* address_field);
  static bool ParseState(AutofillScanner* scanner,
                         bool is_ecml,
                         AddressField* address_field);

  // Looks for an address type in the given text, which the caller must
  // convert to lowercase.
  static AddressType AddressTypeFromText(const string16& text);

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
