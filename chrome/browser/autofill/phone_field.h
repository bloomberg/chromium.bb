// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_

#include <vector>

#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"

class AutoFillField;

// A phone number in one of the following formats:
// - area code, prefix, suffix
// - area code, number
// - number
class PhoneField : public FormField {
 public:
  static PhoneField* Parse(std::vector<AutoFillField*>::const_iterator* iter,
                           bool is_ecml);
  static PhoneField* ParseECML(
      std::vector<AutoFillField*>::const_iterator* iter);

  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;

  virtual int priority() const { return 2; }

 protected:
  PhoneField();

 private:
  // Always present; holds suffix if prefix is present.
  AutoFillField* phone_;

  AutoFillField* area_code_;  // optional
  AutoFillField* prefix_;     // optional
  AutoFillField* extension_;  // optional
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
