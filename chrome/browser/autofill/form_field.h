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

// Field names from the ECML specification; see RFC 3106.  We've
// made these names lowercase since we convert labels and field names to
// lowercase before searching.

// shipping name/address fields
static string16 kEcmlShipToTitle = ASCIIToUTF16("ecom_shipto_postal_name_prefix");
static string16 kEcmlShipToFirstName = ASCIIToUTF16("ecom_shipto_postal_name_first");
static string16 kEcmlShipToMiddleName = ASCIIToUTF16("ecom_shipto_postal_name_middle");
static string16 kEcmlShipToLastName = ASCIIToUTF16("ecom_shipto_postal_name_last");
static string16 kEcmlShipToNameSuffix = ASCIIToUTF16("ecom_shipto_postal_name_suffix");
static string16 kEcmlShipToCompanyName = ASCIIToUTF16("ecom_shipto_postal_company");
static string16 kEcmlShipToAddress1 = ASCIIToUTF16("ecom_shipto_postal_street_line1");
static string16 kEcmlShipToAddress2 = ASCIIToUTF16("ecom_shipto_postal_street_line2");
static string16 kEcmlShipToAddress3 = ASCIIToUTF16("ecom_shipto_postal_street_line3");
static string16 kEcmlShipToCity = ASCIIToUTF16("ecom_shipto_postal_city");
static string16 kEcmlShipToStateProv = ASCIIToUTF16("ecom_shipto_postal_stateprov");
static string16 kEcmlShipToPostalCode = ASCIIToUTF16("ecom_shipto_postal_postalcode");
static string16 kEcmlShipToCountry = ASCIIToUTF16("ecom_shipto_postal_countrycode");
static string16 kEcmlShipToPhone = ASCIIToUTF16("ecom_shipto_telecom_phone_number");
static string16 kEcmlShipToEmail = ASCIIToUTF16("ecom_shipto_online_email");

  // billing name/address fields
static string16 kEcmlBillToTitle = ASCIIToUTF16("ecom_billto_postal_name_prefix");
static string16 kEcmlBillToFirstName = ASCIIToUTF16("ecom_billto_postal_name_first");
static string16 kEcmlBillToMiddleName = ASCIIToUTF16("ecom_billto_postal_name_middle");
static string16 kEcmlBillToLastName = ASCIIToUTF16("ecom_billto_postal_name_last");
static string16 kEcmlBillToNameSuffix = ASCIIToUTF16("ecom_billto_postal_name_suffix");
static string16 kEcmlBillToCompanyName = ASCIIToUTF16("ecom_billto_postal_company");
static string16 kEcmlBillToAddress1 = ASCIIToUTF16("ecom_billto_postal_street_line1");
static string16 kEcmlBillToAddress2 = ASCIIToUTF16("ecom_billto_postal_street_line2");
static string16 kEcmlBillToAddress3 = ASCIIToUTF16("ecom_billto_postal_street_line3");
static string16 kEcmlBillToCity = ASCIIToUTF16("ecom_billto_postal_city");
static string16 kEcmlBillToStateProv = ASCIIToUTF16("ecom_billto_postal_stateprov");
static string16 kEcmlBillToPostalCode = ASCIIToUTF16("ecom_billto_postal_postalcode");
static string16 kEcmlBillToCountry = ASCIIToUTF16("ecom_billto_postal_countrycode");
static string16 kEcmlBillToPhone = ASCIIToUTF16("ecom_billto_telecom_phone_number");
static string16 kEcmlBillToEmail = ASCIIToUTF16("ecom_billto_online_email");

  // credit card fields
static string16 kEcmlCardHolder = ASCIIToUTF16("ecom_payment_card_name");
static string16 kEcmlCardType = ASCIIToUTF16("ecom_payment_card_type");
static string16 kEcmlCardNumber = ASCIIToUTF16("ecom_payment_card_number");
static string16 kEcmlCardVerification = ASCIIToUTF16("ecom_payment_card_verification");
static string16 kEcmlCardExpireDay = ASCIIToUTF16("ecom_payment_card_expdate_day");
static string16 kEcmlCardExpireMonth = ASCIIToUTF16("ecom_payment_card_expdate_month");
static string16 kEcmlCardExpireYear = ASCIIToUTF16("ecom_payment_card_expdate_year");

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

  // TODO(jhawkins): I'm pretty sure this is not necessary in AutoFill++ because
  // we use a static dialog setup.
  virtual int priority() const = 0;

  // Returns true if |field| contains the regexp |pattern| in the name or label.
  static bool Match(AutoFillField* field, const string16& pattern);

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

  // Attempts to parse a control with an empty label.
  static bool ParseEmpty(std::vector<AutoFillField*>::const_iterator* iter);

  // Adds an association between a field and a type to |field_type_map|.
  static bool Add(FieldTypeMap* field_type_map, AutoFillField* field,
                  const AutoFillType& type);

 protected:
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
};

class FormFieldSet : public std::vector<FormField*> {
 public:
  explicit FormFieldSet(FormStructure* form);

  class FormFieldSorter {
  public:
    bool operator() (FormField *v1, FormField *v2) {
      return v1->priority() < v2->priority();
    }
  };

  ~FormFieldSet() {
    for (iterator i = begin(); i != end(); ++i)
      delete *i;
  }

 private:
  // Checks if any of the labels are named according to the ECML standard.
  // Returns true if at least one ECML named element is found.
  bool CheckECML(FormStructure* fields);
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_FIELD_H_
