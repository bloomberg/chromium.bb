// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_

#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_structure.h"

extern string16 kEcmlShipToTitle;
extern string16 kEcmlShipToFirstName;
extern string16 kEcmlShipToMiddleName;
extern string16 kEcmlShipToLastName;
extern string16 kEcmlShipToNameSuffix;
extern string16 kEcmlShipToCompanyName;
extern string16 kEcmlShipToAddress1;
extern string16 kEcmlShipToAddress2;
extern string16 kEcmlShipToAddress3;
extern string16 kEcmlShipToCity;
extern string16 kEcmlShipToStateProv;
extern string16 kEcmlShipToPostalCode;
extern string16 kEcmlShipToCountry;
extern string16 kEcmlShipToPhone;
extern string16 kEcmlShipToEmail;

// billing name/address fields
extern string16 kEcmlBillToTitle;
extern string16 kEcmlBillToFirstName;
extern string16 kEcmlBillToMiddleName;
extern string16 kEcmlBillToLastName;
extern string16 kEcmlBillToNameSuffix;
extern string16 kEcmlBillToCompanyName;
extern string16 kEcmlBillToAddress1;
extern string16 kEcmlBillToAddress2;
extern string16 kEcmlBillToAddress3;
extern string16 kEcmlBillToCity;
extern string16 kEcmlBillToStateProv;
extern string16 kEcmlBillToPostalCode;
extern string16 kEcmlBillToCountry;
extern string16 kEcmlBillToPhone;
extern string16 kEcmlBillToEmail;

// credit card fields
extern string16 kEcmlCardHolder;
extern string16 kEcmlCardType;
extern string16 kEcmlCardNumber;
extern string16 kEcmlCardVerification;
extern string16 kEcmlCardExpireDay;
extern string16 kEcmlCardExpireMonth;
extern string16 kEcmlCardExpireYear;

enum FormFieldType {
  kAddressType,
  kCreditCardType,
  kOtherFieldType
};

class AutoFillField;

// Represents a logical form field in a web form.  Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  virtual ~FormField() {}

  // Associates the available AutoFillTypes of a FormField into
  // |field_type_map|.
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const = 0;

  // Returns the type of form field of the class implementing this interface.
  virtual FormFieldType GetFormFieldType() const { return kOtherFieldType; }

  // Returns true if |field| contains the regexp |pattern| in the name or label.
  // If |match_label_only| is true, then only the field's label is considered.
  static bool Match(AutoFillField* field,
                    const string16& pattern,
                    bool match_label_only);

  // Parses a field using the different field views we know about.  |is_ecml|
  // should be true when the field conforms to the ECML specification.
  static FormField* ParseFormField(
      std::vector<AutoFillField*>::const_iterator* field,
      bool is_ecml);

  // Attempts to parse a text field with the given pattern; returns true on
  // success, but doesn't return the actual text field itself.
  static bool ParseText(std::vector<AutoFillField*>::const_iterator* iter,
                        const string16& pattern);

  // Attempts to parse a text field with the given pattern.  Returns true on
  // success and fills |dest| with a pointer to the field.
  static bool ParseText(std::vector<AutoFillField*>::const_iterator* iter,
                        const string16& pattern,
                        AutoFillField** dest);

  // Attempts to parse a text field with an empty name or label.  Returns true
  // on success and fills |dest| with a pointer to the field.
  static bool ParseEmptyText(std::vector<AutoFillField*>::const_iterator* iter,
                             AutoFillField** dest);

  // Attempts to parse a text field label with the given pattern.  Returns true
  // on success and fills |dest| with a pointer to the field.
  static bool ParseLabelText(std::vector<AutoFillField*>::const_iterator* iter,
                             const string16& pattern,
                             AutoFillField** dest);

  // Attempts to parse a control with an empty label.
  static bool ParseEmpty(std::vector<AutoFillField*>::const_iterator* iter);

  // Adds an association between a field and a type to |field_type_map|.
  static bool Add(FieldTypeMap* field_type_map, AutoFillField* field,
                  const AutoFillType& type);

 protected:
  // Only derived classes may instantiate.
  FormField() {}

  // Note: ECML compliance checking has been modified to accommodate Google
  // Checkout field name limitation. All ECML compliant web forms will be
  // recognized correctly as such however the restrictions on having exactly
  // ECML compliant names have been loosened to only require that field names
  // be prefixed with an ECML compiant name in order to accommodate checkout.
  // Additionally we allow the use of '.' as a word delimiter in addition to the
  // ECML standard '_' (see FormField::FormField for details).
  static string16 GetEcmlPattern(const string16& ecml_name);
  static string16 GetEcmlPattern(const string16& ecml_name1,
                                 const string16& ecml_name2,
                                 string16::value_type pattern_operator);

 private:
  static bool ParseText(std::vector<AutoFillField*>::const_iterator* iter,
                        const string16& pattern,
                        AutoFillField** dest,
                        bool match_label_only);

  // For empty strings we need to test that both label and name are empty.
  static bool ParseLabelAndName(
      std::vector<AutoFillField*>::const_iterator* iter,
      const string16& pattern,
      AutoFillField** dest);
  static bool MatchName(AutoFillField* field, const string16& pattern);
  static bool MatchLabel(AutoFillField* field, const string16& pattern);

  DISALLOW_COPY_AND_ASSIGN(FormField);
};

class FormFieldSet : public std::vector<FormField*> {
 public:
  explicit FormFieldSet(FormStructure* form);

  ~FormFieldSet() {
    for (iterator i = begin(); i != end(); ++i)
      delete *i;
  }

 private:
  // Checks if any of the labels are named according to the ECML standard.
  // Returns true if at least one ECML named element is found.
  bool CheckECML(FormStructure* fields);

  DISALLOW_COPY_AND_ASSIGN(FormFieldSet);
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
