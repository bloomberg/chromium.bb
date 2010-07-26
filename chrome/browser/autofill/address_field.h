// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"

class AutoFillField;

class AddressField : public FormField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;
  virtual FormFieldType GetFormFieldType() const { return kAddressType; }

  static AddressField* Parse(std::vector<AutoFillField*>::const_iterator* iter,
                             bool is_ecml);

  // Tries to determine the billing/shipping type of this address.
  AddressType FindType() const;

  void SetType(AddressType address_type) { type_ = address_type; }

  // Returns true if this is a full address as opposed to an address fragment
  // such as a stand-alone ZIP code.
  bool IsFullAddress() { return address1_ != NULL; }

 private:
  AddressField();

  static bool ParseCompany(std::vector<AutoFillField*>::const_iterator* iter,
                           bool is_ecml, AddressField* address_field);
  static bool ParseAddressLines(
      std::vector<AutoFillField*>::const_iterator* iter,
      bool is_ecml, AddressField* address_field);
  static bool ParseCountry(std::vector<AutoFillField*>::const_iterator* iter,
                           bool is_ecml, AddressField* address_field);
  static bool ParseZipCode(std::vector<AutoFillField*>::const_iterator* iter,
                           bool is_ecml, AddressField* address_field);
  static bool ParseCity(std::vector<AutoFillField*>::const_iterator* iter,
                        bool is_ecml, AddressField* address_field);
  bool ParseState(std::vector<AutoFillField*>::const_iterator* iter,
                  bool is_ecml, AddressField* address_field);

  // Looks for an address type in the given text, which the caller must
  // convert to lowercase.
  static AddressType AddressTypeFromText(const string16& text);

  AutoFillField* company_;   // optional
  AutoFillField* address1_;
  AutoFillField* address2_;  // optional
  AutoFillField* city_;
  AutoFillField* state_;     // optional
  AutoFillField* zip_;
  AutoFillField* zip4_;      // optional ZIP+4; we don't fill this yet
  AutoFillField* country_;   // optional

  AddressType type_;
  bool is_ecml_;

  DISALLOW_COPY_AND_ASSIGN(AddressField);
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_FIELD_H_
