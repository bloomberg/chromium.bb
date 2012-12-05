// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_group.h"

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
  virtual void GetMatchingTypes(const string16& text,
                                const std::string& app_locale,
                                FieldTypeSet* matching_types) const OVERRIDE;

  const std::string& country_code() const { return country_code_; }
  void set_country_code(const std::string& country_code) {
    country_code_ = country_code;
  }

 private:
  // FormGroup:
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const OVERRIDE;

  // Returns the localized country name corresponding to |country_code_|.
  string16 Country() const;

  // The address.
  string16 line1_;
  string16 line2_;
  string16 city_;
  string16 state_;
  std::string country_code_;
  string16 zip_code_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_H_
