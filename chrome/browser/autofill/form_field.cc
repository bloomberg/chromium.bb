// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_field.h"

#include "chrome/browser/autofill/address_field.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/phone_field.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

class EmailField : public FormField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const {
    bool ok = Add(field_type_map, field_, AutoFillType(EMAIL_ADDRESS));
    DCHECK(ok);
    return true;
  }

  static EmailField* Parse(std::vector<AutoFillField*>::const_iterator* iter,
                          bool is_ecml) {
    string16 pattern;
    if (is_ecml) {
      pattern = GetEcmlPattern(kEcmlShipToEmail, kEcmlBillToEmail, '|');
    } else {
      pattern = ASCIIToUTF16("email|e-mail");
    }

    AutoFillField* field;
    if (ParseText(iter, pattern, &field))
      return new EmailField(field);

    return NULL;
  }

  virtual int priority() const { return 1; }

 private:
  explicit EmailField(AutoFillField *field) : field_(field) {}

  AutoFillField* field_;
};

// static
bool FormField::Match(AutoFillField* field, const string16& pattern) {
  WebKit::WebRegularExpression re(WebKit::WebString(pattern),
                                  WebKit::WebTextCaseInsensitive);

  // For now, we apply the same pattern to the field's label and the field's
  // name.  Matching the name is a bit of a long shot for many patterns, but
  // it generally doesn't hurt to try.
  if (re.match(WebKit::WebString(field->label())) ||
      re.match(WebKit::WebString(field->name()))) {
    return true;
  }

  return false;
}

// static
FormField* FormField::ParseFormField(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml) {
  FormField *field;
  field = EmailField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;
  field = PhoneField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;
  field = AddressField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;

  // TODO(jhawkins):
  //  - CreditCardField
  //  - NameField

  return NULL;
}

// static
bool FormField::ParseText(std::vector<AutoFillField*>::const_iterator* iter,
                          const string16& pattern) {
  AutoFillField* field;
  return ParseText(iter, pattern, &field);
}

// static
bool FormField::ParseText(std::vector<AutoFillField*>::const_iterator* iter,
                          const string16& pattern,
                          AutoFillField** dest) {
  AutoFillField* field = **iter;
  if (!field)
    return false;

  if (Match(field, pattern)) {
    *dest = field;
    (*iter)++;
    return true;
  }

  return false;
}

// static
bool FormField::ParseEmpty(std::vector<AutoFillField*>::const_iterator* iter) {
  // TODO(jhawkins): Handle select fields.
  return ParseText(iter, ASCIIToUTF16(""));
}

// static
bool FormField::Add(FieldTypeMap* field_type_map, AutoFillField* field,
               const AutoFillType& type) {
  // Several fields are optional.
  if (field)
    field_type_map->insert(make_pair(field->unique_name(), type.field_type()));

  return true;
}

string16 FormField::GetEcmlPattern(const string16& ecml_name) {
  return ASCIIToUTF16("&") + ecml_name;
}

string16 FormField::GetEcmlPattern(const string16& ecml_name1,
                                   const string16& ecml_name2,
                                   string16::value_type pattern_operator) {
  string16 ampersand = ASCIIToUTF16("&");
  return ampersand + ecml_name1 + pattern_operator + ampersand + ecml_name2;
}

FormFieldSet::FormFieldSet(FormStructure* fields) {
  bool is_ecml = CheckECML(fields);

  // Parse fields.
  std::vector<AutoFillField*>::const_iterator field = fields->begin();
  while (field != fields->end() && *field != NULL) {
    FormField* form_field = FormField::ParseFormField(&field, is_ecml);
    if (!form_field) {
      field++;
      continue;
    }

    push_back(form_field);
  }
}

bool FormFieldSet::CheckECML(FormStructure* fields) {
  size_t num_fields = fields->field_count();
  for (size_t index = 0; index < num_fields; ++index) {
    string16 name(fields->field(index)->name());
    if (name.substr(0, 4) == ASCIIToUTF16("ecom") &&
        (StartsWith(name, kEcmlShipToTitle, false) ||
         StartsWith(name, kEcmlShipToFirstName, false) ||
         StartsWith(name, kEcmlShipToMiddleName, false) ||
         StartsWith(name, kEcmlShipToLastName, false) ||
         StartsWith(name, kEcmlShipToNameSuffix, false) ||
         StartsWith(name, kEcmlShipToCompanyName, false) ||
         StartsWith(name, kEcmlShipToAddress1, false) ||
         StartsWith(name, kEcmlShipToAddress2, false) ||
         StartsWith(name, kEcmlShipToAddress3, false) ||
         StartsWith(name, kEcmlShipToCity, false) ||
         StartsWith(name, kEcmlShipToStateProv, false) ||
         StartsWith(name, kEcmlShipToPostalCode, false) ||
         StartsWith(name, kEcmlShipToCountry, false) ||
         StartsWith(name, kEcmlShipToPhone, false) ||
         StartsWith(name, kEcmlShipToPhone, false) ||
         StartsWith(name, kEcmlShipToEmail, false) ||
         StartsWith(name, kEcmlBillToTitle, false) ||
         StartsWith(name, kEcmlBillToFirstName, false) ||
         StartsWith(name, kEcmlBillToMiddleName, false) ||
         StartsWith(name, kEcmlBillToLastName, false) ||
         StartsWith(name, kEcmlBillToNameSuffix, false) ||
         StartsWith(name, kEcmlBillToCompanyName, false) ||
         StartsWith(name, kEcmlBillToAddress1, false) ||
         StartsWith(name, kEcmlBillToAddress2, false) ||
         StartsWith(name, kEcmlBillToAddress3, false) ||
         StartsWith(name, kEcmlBillToCity, false) ||
         StartsWith(name, kEcmlBillToStateProv, false) ||
         StartsWith(name, kEcmlBillToPostalCode, false) ||
         StartsWith(name, kEcmlBillToCountry, false) ||
         StartsWith(name, kEcmlBillToPhone, false) ||
         StartsWith(name, kEcmlBillToPhone, false) ||
         StartsWith(name, kEcmlBillToEmail, false) ||
         StartsWith(name, kEcmlCardHolder, false) ||
         StartsWith(name, kEcmlCardType, false) ||
         StartsWith(name, kEcmlCardNumber, false) ||
         StartsWith(name, kEcmlCardVerification, false) ||
         StartsWith(name, kEcmlCardExpireMonth, false) ||
         StartsWith(name, kEcmlCardExpireYear, false))) {
      return true;
    }
  }
  return false;
}
