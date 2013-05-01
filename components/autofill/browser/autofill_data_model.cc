// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_data_model.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_field.h"
#include "components/autofill/browser/state_names.h"
#include "components/autofill/browser/validation.h"
#include "components/autofill/common/form_field_data.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

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
bool SetSelectControlValue(const base::string16& value,
                           FormFieldData* field) {
  base::string16 value_lowercase = StringToLowerASCII(value);

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

bool FillStateSelectControl(const base::string16& value,
                            FormFieldData* field) {
  base::string16 abbreviation = value;
  base::string16 full = state_names::GetNameForAbbreviation(value);
  if (full.empty()) {
    abbreviation = state_names::GetAbbreviationForName(value);
    full = value;
  }

  // Try the abbreviation first.
  if (!abbreviation.empty() && SetSelectControlValue(abbreviation, field))
    return true;

  return !full.empty() && SetSelectControlValue(full, field);
}

bool FillExpirationMonthSelectControl(const base::string16& value,
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
bool FillCreditCardTypeSelectControl(const base::string16& value,
                                     FormFieldData* field) {
  // Try stripping off spaces.
  base::string16 value_stripped;
  RemoveChars(StringToLowerASCII(value), kWhitespaceUTF16, &value_stripped);

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    base::string16 option_value_lowercase;
    RemoveChars(StringToLowerASCII(field->option_values[i]), kWhitespaceUTF16,
                &option_value_lowercase);
    base::string16 option_contents_lowercase;
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

AutofillDataModel::AutofillDataModel(const std::string& guid,
                                     const std::string& origin)
    : guid_(guid),
      origin_(origin) {}
AutofillDataModel::~AutofillDataModel() {}

void AutofillDataModel::FillSelectControl(AutofillFieldType type,
                                          const std::string& app_locale,
                                          FormFieldData* field) const {
  DCHECK(field);
  DCHECK_EQ("select-one", field->form_control_type);
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());

  base::string16 field_text = GetInfo(type, app_locale);
  base::string16 field_text_lower = StringToLowerASCII(field_text);
  if (field_text.empty())
    return;

  base::string16 value;
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
    FillCountrySelectControl(app_locale, field);
  } else if (type == CREDIT_CARD_EXP_MONTH) {
    FillExpirationMonthSelectControl(field_text, field);
  } else if (type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    // Attempt to fill the year as a 2-digit year.  This compensates for the
    // fact that our heuristics do not always correctly detect when a website
    // requests a 2-digit rather than a 4-digit year.
    FillSelectControl(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale, field);
  } else if (type == CREDIT_CARD_TYPE) {
    FillCreditCardTypeSelectControl(field_text, field);
  }
}

bool AutofillDataModel::FillCountrySelectControl(
    const std::string& app_locale,
    FormFieldData* field_data) const {
  return false;
}

bool AutofillDataModel::IsVerified() const {
  return !origin_.empty() && !GURL(origin_).is_valid();
}

}  // namespace autofill
