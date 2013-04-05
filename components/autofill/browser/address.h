// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_
#define COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/form_group.h"

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address();
  Address(const Address& address);
  virtual ~Address();

  Address& operator=(const Address& address);

  // FormGroup:
  virtual string16 GetRawInfo(AutofillFieldType type) const OVERRIDE;
  virtual void SetRawInfo(AutofillFieldType type,
                          const string16& value) OVERRIDE;
  virtual string16 GetInfo(AutofillFieldType type,
                           const std::string& app_locale) const OVERRIDE;
  virtual bool SetInfo(AutofillFieldType type,
                       const string16& value,
                       const std::string& app_locale) OVERRIDE;
  virtual void GetMatchingTypes(const string16& text,
                                const std::string& app_locale,
                                FieldTypeSet* matching_types) const OVERRIDE;

 private:
  // FormGroup:
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const OVERRIDE;

  // The address.
  string16 line1_;
  string16 line2_;
  string16 city_;
  string16 state_;
  string16 country_code_;
  string16 zip_code_;
};

#endif  // COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_
