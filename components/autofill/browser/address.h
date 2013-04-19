// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_
#define COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/form_group.h"

namespace autofill {

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address();
  Address(const Address& address);
  virtual ~Address();

  Address& operator=(const Address& address);

  // FormGroup:
  virtual base::string16 GetRawInfo(AutofillFieldType type) const OVERRIDE;
  virtual void SetRawInfo(AutofillFieldType type,
                          const base::string16& value) OVERRIDE;
  virtual base::string16 GetInfo(AutofillFieldType type,
                           const std::string& app_locale) const OVERRIDE;
  virtual bool SetInfo(AutofillFieldType type,
                       const base::string16& value,
                       const std::string& app_locale) OVERRIDE;
  virtual void GetMatchingTypes(const base::string16& text,
                                const std::string& app_locale,
                                FieldTypeSet* matching_types) const OVERRIDE;

 private:
  // FormGroup:
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const OVERRIDE;

  // The address.
  base::string16 line1_;
  base::string16 line2_;
  base::string16 city_;
  base::string16 state_;
  base::string16 country_code_;
  base::string16 zip_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_ADDRESS_H_
