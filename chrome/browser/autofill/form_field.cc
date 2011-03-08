// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_field.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address_field.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/credit_card_field.h"
#include "chrome/browser/autofill/name_field.h"
#include "chrome/browser/autofill/phone_field.h"
#include "grit/autofill_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "ui/base/l10n/l10n_util.h"

// Field names from the ECML specification; see RFC 3106.  We've
// made these names lowercase since we convert labels and field names to
// lowercase before searching.

// shipping name/address fields
const char kEcmlShipToTitle[] = "ecom_shipto_postal_name_prefix";
const char kEcmlShipToFirstName[] = "ecom_shipto_postal_name_first";
const char kEcmlShipToMiddleName[] = "ecom_shipto_postal_name_middle";
const char kEcmlShipToLastName[] = "ecom_shipto_postal_name_last";
const char kEcmlShipToNameSuffix[] = "ecom_shipto_postal_name_suffix";
const char kEcmlShipToCompanyName[] = "ecom_shipto_postal_company";
const char kEcmlShipToAddress1[] = "ecom_shipto_postal_street_line1";
const char kEcmlShipToAddress2[] = "ecom_shipto_postal_street_line2";
const char kEcmlShipToAddress3[] = "ecom_shipto_postal_street_line3";
const char kEcmlShipToCity[] = "ecom_shipto_postal_city";
const char kEcmlShipToStateProv[] = "ecom_shipto_postal_stateprov";
const char kEcmlShipToPostalCode[] = "ecom_shipto_postal_postalcode";
const char kEcmlShipToCountry[] = "ecom_shipto_postal_countrycode";
const char kEcmlShipToPhone[] = "ecom_shipto_telecom_phone_number";
const char kEcmlShipToEmail[] = "ecom_shipto_online_email";

// billing name/address fields
const char kEcmlBillToTitle[] = "ecom_billto_postal_name_prefix";
const char kEcmlBillToFirstName[] = "ecom_billto_postal_name_first";
const char kEcmlBillToMiddleName[] = "ecom_billto_postal_name_middle";
const char kEcmlBillToLastName[] = "ecom_billto_postal_name_last";
const char kEcmlBillToNameSuffix[] = "ecom_billto_postal_name_suffix";
const char kEcmlBillToCompanyName[] = "ecom_billto_postal_company";
const char kEcmlBillToAddress1[] = "ecom_billto_postal_street_line1";
const char kEcmlBillToAddress2[] = "ecom_billto_postal_street_line2";
const char kEcmlBillToAddress3[] = "ecom_billto_postal_street_line3";
const char kEcmlBillToCity[] = "ecom_billto_postal_city";
const char kEcmlBillToStateProv[] = "ecom_billto_postal_stateprov";
const char kEcmlBillToPostalCode[] = "ecom_billto_postal_postalcode";
const char kEcmlBillToCountry[] = "ecom_billto_postal_countrycode";
const char kEcmlBillToPhone[] = "ecom_billto_telecom_phone_number";
const char kEcmlBillToEmail[] = "ecom_billto_online_email";

// credit card fields
const char kEcmlCardHolder[] = "ecom_payment_card_name";
const char kEcmlCardType[] = "ecom_payment_card_type";
const char kEcmlCardNumber[] = "ecom_payment_card_number";
const char kEcmlCardVerification[] = "ecom_payment_card_verification";
const char kEcmlCardExpireDay[] = "ecom_payment_card_expdate_day";
const char kEcmlCardExpireMonth[] = "ecom_payment_card_expdate_month";
const char kEcmlCardExpireYear[] = "ecom_payment_card_expdate_year";

class EmailField : public FormField {
 public:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const {
    bool ok = Add(field_type_map, field_, AutofillType(EMAIL_ADDRESS));
    DCHECK(ok);
    return true;
  }

  static EmailField* Parse(std::vector<AutofillField*>::const_iterator* iter,
                          bool is_ecml) {
    string16 pattern;
    if (is_ecml) {
      pattern = GetEcmlPattern(kEcmlShipToEmail, kEcmlBillToEmail, '|');
    } else {
      pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_EMAIL_RE);
    }

    AutofillField* field;
    if (ParseText(iter, pattern, &field))
      return new EmailField(field);

    return NULL;
  }

 private:
  explicit EmailField(AutofillField *field) : field_(field) {}

  AutofillField* field_;
};

FormFieldType FormField::GetFormFieldType() const {
  return kOtherFieldType;
}

// static
bool FormField::Match(AutofillField* field,
                      const string16& pattern,
                      bool match_label_only) {
  if (match_label_only) {
    if (MatchLabel(field, pattern)) {
      return true;
    }
  } else {
    // For now, we apply the same pattern to the field's label and the field's
    // name.  Matching the name is a bit of a long shot for many patterns, but
    // it generally doesn't hurt to try.
    if (MatchLabel(field, pattern) || MatchName(field, pattern)) {
      return true;
    }
  }

  return false;
}

// static
bool FormField::MatchName(AutofillField* field, const string16& pattern) {
  // TODO(jhawkins): Remove StringToLowerASCII.  WebRegularExpression needs to
  // be fixed to take WebTextCaseInsensitive into account.
  WebKit::WebRegularExpression re(WebKit::WebString(pattern),
                                  WebKit::WebTextCaseInsensitive);
  bool match = re.match(
      WebKit::WebString(StringToLowerASCII(field->name()))) != -1;
  return match;
}

// static
bool FormField::MatchLabel(AutofillField* field, const string16& pattern) {
  // TODO(jhawkins): Remove StringToLowerASCII.  WebRegularExpression needs to
  // be fixed to take WebTextCaseInsensitive into account.
  WebKit::WebRegularExpression re(WebKit::WebString(pattern),
                                  WebKit::WebTextCaseInsensitive);
  bool match = re.match(
      WebKit::WebString(StringToLowerASCII(field->label()))) != -1;
  return match;
}

// static
FormField* FormField::ParseFormField(
    std::vector<AutofillField*>::const_iterator* iter,
    bool is_ecml) {
  FormField *field;
  field = EmailField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;
  // Parses both phone and fax.
  field = PhoneField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;
  field = AddressField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;
  field = CreditCardField::Parse(iter, is_ecml);
  if (field != NULL)
    return field;

  // We search for a NameField last since it matches the word "name", which is
  // relatively general.
  return NameField::Parse(iter, is_ecml);
}

// static
bool FormField::ParseText(std::vector<AutofillField*>::const_iterator* iter,
                          const string16& pattern) {
  AutofillField* field;
  return ParseText(iter, pattern, &field);
}

// static
bool FormField::ParseText(std::vector<AutofillField*>::const_iterator* iter,
                          const string16& pattern,
                          AutofillField** dest) {
  return ParseText(iter, pattern, dest, false);
}

// static
bool FormField::ParseEmptyText(
    std::vector<AutofillField*>::const_iterator* iter,
    AutofillField** dest) {
  return ParseLabelText(iter, ASCIIToUTF16("^$"), dest);
}

// static
bool FormField::ParseLabelText(
    std::vector<AutofillField*>::const_iterator* iter,
    const string16& pattern,
    AutofillField** dest) {
  return ParseText(iter, pattern, dest, true);
}

// static
bool FormField::ParseText(std::vector<AutofillField*>::const_iterator* iter,
                          const string16& pattern,
                          AutofillField** dest,
                          bool match_label_only) {
  AutofillField* field = **iter;
  if (!field)
    return false;

  if (Match(field, pattern, match_label_only)) {
    if (dest)
      *dest = field;
    (*iter)++;
    return true;
  }

  return false;
}

// static
bool FormField::ParseLabelAndName(
    std::vector<AutofillField*>::const_iterator* iter,
    const string16& pattern,
    AutofillField** dest) {
  AutofillField* field = **iter;
  if (!field)
    return false;

  if (MatchLabel(field, pattern) && MatchName(field, pattern)) {
    if (dest)
      *dest = field;
    (*iter)++;
    return true;
  }

  return false;
}

// static
bool FormField::ParseEmpty(std::vector<AutofillField*>::const_iterator* iter) {
  // TODO(jhawkins): Handle select fields.
  return ParseLabelAndName(iter, ASCIIToUTF16("^$"), NULL);
}

// static
bool FormField::Add(FieldTypeMap* field_type_map, AutofillField* field,
               const AutofillType& type) {
  // Several fields are optional.
  if (field)
    field_type_map->insert(make_pair(field->unique_name(), type.field_type()));

  return true;
}

string16 FormField::GetEcmlPattern(const char* ecml_name) {
  return ASCIIToUTF16(std::string("^") + ecml_name);
}

string16 FormField::GetEcmlPattern(const char* ecml_name1,
                                   const char* ecml_name2,
                                   char pattern_operator) {
  return ASCIIToUTF16(StringPrintf("^%s%c^%s",
      ecml_name1, pattern_operator, ecml_name2));
}

FormFieldSet::FormFieldSet(FormStructure* fields) {
  std::vector<AddressField*> addresses;

  // First, find if there is one form field with an ECML name.  If there is,
  // then we will match an element only if it is in the standard.
  bool is_ecml = CheckECML(fields);

  // Parse fields.
  std::vector<AutofillField*>::const_iterator field = fields->begin();
  while (field != fields->end() && *field != NULL) {
    FormField* form_field = FormField::ParseFormField(&field, is_ecml);
    if (!form_field) {
      field++;
      continue;
    }

    push_back(form_field);

    if (form_field->GetFormFieldType() == kAddressType) {
      AddressField* address = static_cast<AddressField*>(form_field);
      if (address->IsFullAddress())
        addresses.push_back(address);
    }
  }

  // Now determine an address type for each address. Note, if this is an ECML
  // form, then we already got this info from the field names.
  if (!is_ecml && !addresses.empty()) {
    if (addresses.size() == 1) {
      addresses[0]->SetType(addresses[0]->FindType());
    } else {
      AddressType type0 = addresses[0]->FindType();
      AddressType type1 = addresses[1]->FindType();

      // When there are two addresses on a page, they almost always appear in
      // the order (billing, shipping).
      bool reversed = (type0 == kShippingAddress && type1 == kBillingAddress);
      addresses[0]->SetType(reversed ? kShippingAddress : kBillingAddress);
      addresses[1]->SetType(reversed ? kBillingAddress : kShippingAddress);
    }
  }
}

bool FormFieldSet::CheckECML(FormStructure* fields) {
  size_t num_fields = fields->field_count();
  struct EcmlField {
    const char* name_;
    const int length_;
  } form_fields[] = {
#define ECML_STRING_ENTRY(x) { x, arraysize(x) - 1 },
    ECML_STRING_ENTRY(kEcmlShipToTitle)
    ECML_STRING_ENTRY(kEcmlShipToFirstName)
    ECML_STRING_ENTRY(kEcmlShipToMiddleName)
    ECML_STRING_ENTRY(kEcmlShipToLastName)
    ECML_STRING_ENTRY(kEcmlShipToNameSuffix)
    ECML_STRING_ENTRY(kEcmlShipToCompanyName)
    ECML_STRING_ENTRY(kEcmlShipToAddress1)
    ECML_STRING_ENTRY(kEcmlShipToAddress2)
    ECML_STRING_ENTRY(kEcmlShipToAddress3)
    ECML_STRING_ENTRY(kEcmlShipToCity)
    ECML_STRING_ENTRY(kEcmlShipToStateProv)
    ECML_STRING_ENTRY(kEcmlShipToPostalCode)
    ECML_STRING_ENTRY(kEcmlShipToCountry)
    ECML_STRING_ENTRY(kEcmlShipToPhone)
    ECML_STRING_ENTRY(kEcmlShipToPhone)
    ECML_STRING_ENTRY(kEcmlShipToEmail)
    ECML_STRING_ENTRY(kEcmlBillToTitle)
    ECML_STRING_ENTRY(kEcmlBillToFirstName)
    ECML_STRING_ENTRY(kEcmlBillToMiddleName)
    ECML_STRING_ENTRY(kEcmlBillToLastName)
    ECML_STRING_ENTRY(kEcmlBillToNameSuffix)
    ECML_STRING_ENTRY(kEcmlBillToCompanyName)
    ECML_STRING_ENTRY(kEcmlBillToAddress1)
    ECML_STRING_ENTRY(kEcmlBillToAddress2)
    ECML_STRING_ENTRY(kEcmlBillToAddress3)
    ECML_STRING_ENTRY(kEcmlBillToCity)
    ECML_STRING_ENTRY(kEcmlBillToStateProv)
    ECML_STRING_ENTRY(kEcmlBillToPostalCode)
    ECML_STRING_ENTRY(kEcmlBillToCountry)
    ECML_STRING_ENTRY(kEcmlBillToPhone)
    ECML_STRING_ENTRY(kEcmlBillToPhone)
    ECML_STRING_ENTRY(kEcmlBillToEmail)
    ECML_STRING_ENTRY(kEcmlCardHolder)
    ECML_STRING_ENTRY(kEcmlCardType)
    ECML_STRING_ENTRY(kEcmlCardNumber)
    ECML_STRING_ENTRY(kEcmlCardVerification)
    ECML_STRING_ENTRY(kEcmlCardExpireMonth)
    ECML_STRING_ENTRY(kEcmlCardExpireYear)
#undef ECML_STRING_ENTRY
  };

  const string16 ecom(ASCIIToUTF16("ecom"));
  for (size_t index = 0; index < num_fields; ++index) {
    const string16& utf16_name = fields->field(index)->name();
    if (StartsWith(utf16_name, ecom, true)) {
      std::string name(UTF16ToASCII(utf16_name));
      for (size_t i = 0; i < ARRAYSIZE_UNSAFE(form_fields); ++i) {
        if (base::strncasecmp(name.c_str(), form_fields[i].name_,
                              form_fields[i].length_) == 0) {
          return true;
        }
      }
    }
  }

  return false;
}
