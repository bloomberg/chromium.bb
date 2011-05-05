// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/form_field.h"

class AutofillScanner;

// A form field that can parse either a FullNameField or a FirstLastNameField.
class NameField : public FormField {
 public:
  static NameField* Parse(AutofillScanner* scanner, bool is_ecml);

 protected:
  NameField() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NameField);
};

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const OVERRIDE;

  static FullNameField* Parse(AutofillScanner* scanner);

 private:
  explicit FullNameField(const AutofillField* field);

  const AutofillField* field_;
  DISALLOW_COPY_AND_ASSIGN(FullNameField);
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const OVERRIDE;

  static FirstLastNameField* ParseSpecificName(AutofillScanner* scanner);
  static FirstLastNameField* ParseComponentNames(AutofillScanner* scanner);
  static FirstLastNameField* ParseEcmlName(AutofillScanner* scanner);
  static FirstLastNameField* Parse(AutofillScanner* scanner, bool is_ecml);

 private:
  FirstLastNameField();

  const AutofillField* first_name_;
  const AutofillField* middle_name_;  // Optional.
  const AutofillField* last_name_;
  bool middle_initial_;  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstLastNameField);
};

#endif  // CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
