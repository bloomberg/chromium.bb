// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_group.h"

#include <algorithm>
#include <iterator>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/common/form_field_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// TODO(jhawkins): Add more states/provinces.  See http://crbug.com/45039.

class State {
 public:
  const char* name;
  const char* abbreviation;

  static const State all_states[];

  static string16 Abbreviation(const string16& name);
  static string16 FullName(const string16& abbreviation);
};

const State State::all_states[] = {
  { "alabama", "al" },
  { "alaska", "ak" },
  { "arizona", "az" },
  { "arkansas", "ar" },
  { "california", "ca" },
  { "colorado", "co" },
  { "connecticut", "ct" },
  { "delaware", "de" },
  { "district of columbia", "dc" },
  { "florida", "fl" },
  { "georgia", "ga" },
  { "hawaii", "hi" },
  { "idaho", "id" },
  { "illinois", "il" },
  { "indiana", "in" },
  { "iowa", "ia" },
  { "kansas", "ks" },
  { "kentucky", "ky" },
  { "louisiana", "la" },
  { "maine", "me" },
  { "maryland", "md" },
  { "massachusetts", "ma" },
  { "michigan", "mi" },
  { "minnesota", "mv" },
  { "mississippi", "ms" },
  { "missouri", "mo" },
  { "montana", "mt" },
  { "nebraska", "ne" },
  { "nevada", "nv" },
  { "new hampshire", "nh" },
  { "new jersey", "nj" },
  { "new mexico", "nm" },
  { "new york", "ny" },
  { "north carolina", "nc" },
  { "north dakota", "nd" },
  { "ohio", "oh" },
  { "oklahoma", "ok" },
  { "oregon", "or" },
  { "pennsylvania", "pa" },
  { "puerto rico", "pr" },
  { "rhode island", "ri" },
  { "south carolina", "sc" },
  { "south dakota", "sd" },
  { "tennessee", "tn" },
  { "texas", "tx" },
  { "utah", "ut" },
  { "vermont", "vt" },
  { "virginia", "va" },
  { "washington", "wa" },
  { "west virginia", "wv" },
  { "wisconsin", "wi" },
  { "wyoming", "wy" },
  { NULL, NULL }
};

string16 State::Abbreviation(const string16& name) {
  for (const State* state = all_states; state->name; ++state) {
    if (LowerCaseEqualsASCII(name, state->name))
      return ASCIIToUTF16(state->abbreviation);
  }
  return string16();
}

string16 State::FullName(const string16& abbreviation) {
  for (const State* state = all_states; state->name; ++state) {
    if (LowerCaseEqualsASCII(abbreviation, state->abbreviation))
      return ASCIIToUTF16(state->name);
  }
  return string16();
}

const char* const kMonthsAbbreviated[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

const char* const kMonthsFull[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
};

const char* const kMonthsNumeric[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
};

// Returns true if the value was successfully set, meaning |value| was found in
// the list of select options in |field|.
bool SetSelectControlValue(const string16& value,
                           FormFieldData* field) {
  string16 value_lowercase = StringToLowerASCII(value);

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (value_lowercase == StringToLowerASCII(field->option_values[i]) ||
        value_lowercase == StringToLowerASCII(field->option_contents[i])) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

bool FillStateSelectControl(const string16& value,
                            FormFieldData* field) {
  string16 abbrev, full;
  if (value.size() < 4U) {
    abbrev = value;
    full = State::FullName(value);
  } else {
    abbrev = State::Abbreviation(value);
    full = value;
  }

  // Try the abbreviation name first.
  if (!abbrev.empty() && SetSelectControlValue(abbrev, field))
    return true;

  if (full.empty())
    return false;

  return SetSelectControlValue(full, field);
}

bool FillExpirationMonthSelectControl(const string16& value,
                                      FormFieldData* field) {
  int index = 0;
  if (!base::StringToInt(value, &index) ||
      index <= 0 ||
      static_cast<size_t>(index) >= arraysize(kMonthsFull))
    return false;

  bool filled =
      SetSelectControlValue(ASCIIToUTF16(kMonthsAbbreviated[index]), field) ||
      SetSelectControlValue(ASCIIToUTF16(kMonthsFull[index]), field) ||
      SetSelectControlValue(ASCIIToUTF16(kMonthsNumeric[index]), field);
  return filled;
}

// Try to fill a credit card type |value| (Visa, MasterCard, etc.) into the
// given |field|.
bool FillCreditCardTypeSelectControl(const string16& value,
                                     FormFieldData* field) {
  // Try stripping off spaces.
  string16 value_stripped;
  RemoveChars(StringToLowerASCII(value), kWhitespaceUTF16, &value_stripped);

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    string16 option_value_lowercase;
    RemoveChars(StringToLowerASCII(field->option_values[i]), kWhitespaceUTF16,
                &option_value_lowercase);
    string16 option_contents_lowercase;
    RemoveChars(StringToLowerASCII(field->option_contents[i]), kWhitespaceUTF16,
                &option_contents_lowercase);

    // Perform a case-insensitive comparison; but fill the form with the
    // original text, not the lowercased version.
    if (value_stripped == option_value_lowercase ||
        value_stripped == option_contents_lowercase) {
      field->value = field->option_values[i];
      return true;
    }
  }

  // For American Express, also try filling as "AmEx".
  if (value == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX))
    return FillCreditCardTypeSelectControl(ASCIIToUTF16("AmEx"), field);

  return false;
}

}  // namespace

std::string FormGroup::GetGUID() const {
  NOTREACHED();
  return std::string();
}

void FormGroup::GetMatchingTypes(const string16& text,
                                 const std::string& app_locale,
                                 FieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  FieldTypeSet types;
  GetSupportedTypes(&types);
  for (FieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    // TODO(isherman): Matches are case-sensitive for now.  Let's keep an eye on
    // this and decide whether there are compelling reasons to add case-
    // insensitivity.
    if (GetInfo(*type, app_locale) == text)
      matching_types->insert(*type);
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
                                 FieldTypeSet* non_empty_types) const {
  FieldTypeSet types;
  GetSupportedTypes(&types);
  for (FieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    if (!GetInfo(*type, app_locale).empty())
      non_empty_types->insert(*type);
  }
}

string16 FormGroup::GetInfo(AutofillFieldType type,
                            const std::string& app_locale) const {
  return GetRawInfo(type);
}

bool FormGroup::SetInfo(AutofillFieldType type,
                        const string16& value,
                        const std::string& app_locale) {
  SetRawInfo(type, value);
  return true;
}

void FormGroup::FillFormField(const AutofillField& field,
                              size_t variant,
                              FormFieldData* field_data) const {
  NOTREACHED();
}

void FormGroup::FillSelectControl(AutofillFieldType type,
                                  FormFieldData* field) const {
  DCHECK(field);
  DCHECK_EQ("select-one", field->form_control_type);
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());

  const std::string app_locale = AutofillCountry::ApplicationLocale();
  string16 field_text = GetInfo(type, app_locale);
  string16 field_text_lower = StringToLowerASCII(field_text);
  if (field_text.empty())
    return;

  string16 value;
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (field_text == field->option_values[i] ||
        field_text == field->option_contents[i]) {
      // An exact match, use it.
      value = field->option_values[i];
      break;
    }

    if (field_text_lower == StringToLowerASCII(field->option_values[i]) ||
        field_text_lower == StringToLowerASCII(field->option_contents[i])) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      value = field->option_values[i];
    }
  }

  if (!value.empty()) {
    field->value = value;
    return;
  }

  if (type == ADDRESS_HOME_STATE || type == ADDRESS_BILLING_STATE) {
    FillStateSelectControl(field_text, field);
  } else if (type == ADDRESS_HOME_COUNTRY || type == ADDRESS_BILLING_COUNTRY) {
    FillCountrySelectControl(field);
  } else if (type == CREDIT_CARD_EXP_MONTH) {
    FillExpirationMonthSelectControl(field_text, field);
  } else if (type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    // Attempt to fill the year as a 2-digit year.  This compensates for the
    // fact that our heuristics do not always correctly detect when a website
    // requests a 2-digit rather than a 4-digit year.
    FillSelectControl(CREDIT_CARD_EXP_2_DIGIT_YEAR, field);
  } else if (type == CREDIT_CARD_TYPE) {
    FillCreditCardTypeSelectControl(field_text, field);
  }
}

bool FormGroup::FillCountrySelectControl(FormFieldData* field_data) const {
  return false;
}

// static
bool FormGroup::IsValidState(const string16& value) {
  return !State::Abbreviation(value).empty() || !State::FullName(value).empty();
}
