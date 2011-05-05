// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"

class AutofillField;
class AutofillScanner;

extern const char kEcmlShipToTitle[];
extern const char kEcmlShipToFirstName[];
extern const char kEcmlShipToMiddleName[];
extern const char kEcmlShipToLastName[];
extern const char kEcmlShipToNameSuffix[];
extern const char kEcmlShipToCompanyName[];
extern const char kEcmlShipToAddress1[];
extern const char kEcmlShipToAddress2[];
extern const char kEcmlShipToAddress3[];
extern const char kEcmlShipToCity[];
extern const char kEcmlShipToStateProv[];
extern const char kEcmlShipToPostalCode[];
extern const char kEcmlShipToCountry[];
extern const char kEcmlShipToPhone[];
extern const char kEcmlShipToEmail[];

// billing name/address fields
extern const char kEcmlBillToTitle[];
extern const char kEcmlBillToFirstName[];
extern const char kEcmlBillToMiddleName[];
extern const char kEcmlBillToLastName[];
extern const char kEcmlBillToNameSuffix[];
extern const char kEcmlBillToCompanyName[];
extern const char kEcmlBillToAddress1[];
extern const char kEcmlBillToAddress2[];
extern const char kEcmlBillToAddress3[];
extern const char kEcmlBillToCity[];
extern const char kEcmlBillToStateProv[];
extern const char kEcmlBillToPostalCode[];
extern const char kEcmlBillToCountry[];
extern const char kEcmlBillToPhone[];
extern const char kEcmlBillToEmail[];

// credit card fields
extern const char kEcmlCardHolder[];
extern const char kEcmlCardType[];
extern const char kEcmlCardNumber[];
extern const char kEcmlCardVerification[];
extern const char kEcmlCardExpireDay[];
extern const char kEcmlCardExpireMonth[];
extern const char kEcmlCardExpireYear[];

// Represents a logical form field in a web form.  Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  virtual ~FormField() {}

  // Associates each field in |fields| with its heuristically detected type.
  // The association is stored into |field_type_map|.
  static void ParseFormFields(const std::vector<AutofillField*>& fields,
                              FieldTypeMap* field_type_map);

  // Associates the available AutofillTypes of a FormField into
  // |field_type_map|.
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const = 0;

  // Returns true if |field| contains the regexp |pattern| in the name or label.
  // If |match_label_only| is true, then only the field's label is considered.
  static bool Match(const AutofillField* field,
                    const string16& pattern,
                    bool match_label_only);

  // Parses a field using the different field views we know about.  |is_ecml|
  // should be true when the field conforms to the ECML specification.
  static FormField* ParseFormField(AutofillScanner* scanner, bool is_ecml);

  // Attempts to parse a text field with the given pattern; returns true on
  // success, but doesn't return the actual text field itself.
  static bool ParseText(AutofillScanner* scanner, const string16& pattern);

  // Attempts to parse a text field with the given pattern.  Returns true on
  // success and fills |dest| with a pointer to the field.
  static bool ParseText(AutofillScanner* scanner,
                        const string16& pattern,
                        const AutofillField** dest);

  // Attempts to parse a text field with an empty name or label.  Returns true
  // on success and fills |dest| with a pointer to the field.
  static bool ParseEmptyText(AutofillScanner* scanner,
                             const AutofillField** dest);

  // Attempts to parse a text field label with the given pattern.  Returns true
  // on success and fills |dest| with a pointer to the field.
  static bool ParseLabelText(AutofillScanner* scanner,
                             const string16& pattern,
                             const AutofillField** dest);

  // Attempts to parse a control with an empty label.
  static bool ParseEmpty(AutofillScanner* scanner);

  // Adds an association between a field and a type to |field_type_map|.
  static bool Add(FieldTypeMap* field_type_map,
                  const AutofillField* field,
                  AutofillFieldType type);

 protected:
  // Only derived classes may instantiate.
  FormField() {}

  // Note: ECML compliance checking has been modified to accommodate Google
  // Checkout field name limitation. All ECML compliant web forms will be
  // recognized correctly as such however the restrictions on having exactly
  // ECML compliant names have been loosened to only require that field names
  // be prefixed with an ECML compliant name in order to accommodate checkout.
  // Additionally we allow the use of '.' as a word delimiter in addition to the
  // ECML standard '_' (see FormField::FormField for details).
  static string16 GetEcmlPattern(const char* ecml_name);
  static string16 GetEcmlPattern(const char* ecml_name1,
                                 const char* ecml_name2,
                                 char pattern_operator);

 private:
  static bool ParseText(AutofillScanner* scanner,
                        const string16& pattern,
                        const AutofillField** dest,
                        bool match_label_only);

  // For empty strings we need to test that both label and name are empty.
  static bool ParseLabelAndName(AutofillScanner* scanner,
                                const string16& pattern,
                                const AutofillField** dest);
  static bool MatchName(const AutofillField* field, const string16& pattern);
  static bool MatchLabel(const AutofillField* field, const string16& pattern);

  DISALLOW_COPY_AND_ASSIGN(FormField);
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
