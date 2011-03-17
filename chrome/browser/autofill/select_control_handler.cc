// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/select_control_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_group.h"
#include "webkit/glue/form_field.h"

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
  { "west Virginia", "wv" },
  { "wisconsin", "wi" },
  { "wyoming", "wy" },
  { NULL, NULL }
};

string16 State::Abbreviation(const string16& name) {
  for (const State *s = all_states ; s->name ; ++s)
    if (LowerCaseEqualsASCII(name, s->name))
      return ASCIIToUTF16(s->abbreviation);
  return string16();
}

string16 State::FullName(const string16& abbreviation) {
  for (const State *s = all_states ; s->name ; ++s)
    if (LowerCaseEqualsASCII(abbreviation, s->abbreviation))
      return ASCIIToUTF16(s->name);
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
                           webkit_glue::FormField* field) {
  string16 value_lowercase = StringToLowerASCII(value);

  for (std::vector<string16>::const_iterator iter =
           field->option_strings.begin();
       iter != field->option_strings.end(); ++iter) {
    if (value_lowercase == StringToLowerASCII(*iter)) {
      field->value = *iter;
      return true;
    }
  }

  return false;
}

bool FillStateSelectControl(const string16& value,
                            webkit_glue::FormField* field) {
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

bool FillCountrySelectControl(const FormGroup& form_group,
                              webkit_glue::FormField* field) {
  const AutofillProfile& profile =
      static_cast<const AutofillProfile&>(form_group);
  std::string country_code = profile.CountryCode();
  std::string app_locale = AutofillCountry::ApplicationLocale();

  for (std::vector<string16>::const_iterator iter =
           field->option_strings.begin();
       iter != field->option_strings.end();
       ++iter) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code == AutofillCountry::GetCountryCode(*iter, app_locale)) {
      field->value = *iter;
      return true;
    }
  }

  return false;
}

bool FillExpirationMonthSelectControl(const string16& value,
                                      webkit_glue::FormField* field) {
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

}  // namespace

namespace autofill {

void FillSelectControl(const FormGroup& form_group,
                       AutofillFieldType type,
                       webkit_glue::FormField* field) {
  DCHECK(field);
  DCHECK_EQ(ASCIIToUTF16("select-one"), field->form_control_type);

  string16 field_text = form_group.GetInfo(type);
  if (field_text.empty())
    return;

  string16 value;
  for (size_t i = 0; i < field->option_strings.size(); ++i) {
    if (field_text == field->option_strings[i]) {
      // An exact match, use it.
      value = field_text;
      break;
    }

    if (StringToLowerASCII(field->option_strings[i]) ==
        StringToLowerASCII(field_text)) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      value = field->option_strings[i];
    }
  }

  if (!value.empty()) {
    field->value = value;
    return;
  }

  if (type == ADDRESS_HOME_STATE || type == ADDRESS_BILLING_STATE)
    FillStateSelectControl(field_text, field);
  else if (type == ADDRESS_HOME_COUNTRY || type == ADDRESS_BILLING_COUNTRY)
    FillCountrySelectControl(form_group, field);
  else if (type == CREDIT_CARD_EXP_MONTH)
    FillExpirationMonthSelectControl(field_text, field);

  return;
}

bool IsValidState(const string16& value) {
  return !State::Abbreviation(value).empty() || !State::FullName(value).empty();
}

}  // namespace autofill
