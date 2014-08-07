// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/state_names.h"
#include "grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using base::ASCIIToUTF16;
using base::StringToInt;

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

// Returns true if the value was successfully set, meaning |value| was found in
// the list of select options in |field|.
bool SetSelectControlValue(const base::string16& value,
                           FormFieldData* field) {
  base::string16 value_lowercase = base::StringToLowerASCII(value);

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  base::string16 best_match;
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (value == field->option_values[i] ||
        value == field->option_contents[i]) {
      // An exact match, use it.
      best_match = field->option_values[i];
      break;
    }

    if (value_lowercase == base::StringToLowerASCII(field->option_values[i]) ||
        value_lowercase ==
            base::StringToLowerASCII(field->option_contents[i])) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      best_match = field->option_values[i];
    }
  }

  if (best_match.empty())
    return false;

  field->value = best_match;
  return true;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. For example, "NC - North Carolina" would match "north carolina".
bool SetSelectControlValueSubstringMatch(const base::string16& value,
                                         FormFieldData* field) {
  base::string16 value_lowercase = base::StringToLowerASCII(value);
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  int best_match = -1;

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (base::StringToLowerASCII(field->option_values[i]).find(value_lowercase) !=
            std::string::npos ||
        base::StringToLowerASCII(field->option_contents[i]).find(
            value_lowercase) != std::string::npos) {
      // The best match is the shortest one.
      if (best_match == -1 ||
          field->option_values[best_match].size() >
              field->option_values[i].size()) {
        best_match = i;
      }
    }
  }

  if (best_match >= 0) {
    field->value = field->option_values[best_match];
    return true;
  }

  return false;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. First it tokenizes the options, then tries to match against
// tokens. For example, "NC - North Carolina" would match "nc" but not "ca".
bool SetSelectControlValueTokenMatch(const base::string16& value,
                                     FormFieldData* field) {
  base::string16 value_lowercase = base::StringToLowerASCII(value);
  std::vector<base::string16> tokenized;
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    base::SplitStringAlongWhitespace(
        base::StringToLowerASCII(field->option_values[i]), &tokenized);
    if (std::find(tokenized.begin(), tokenized.end(), value_lowercase) !=
        tokenized.end()) {
      field->value = field->option_values[i];
      return true;
    }

    base::SplitStringAlongWhitespace(
        base::StringToLowerASCII(field->option_contents[i]), &tokenized);
    if (std::find(tokenized.begin(), tokenized.end(), value_lowercase) !=
        tokenized.end()) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

// Try to fill a numeric |value| into the given |field|.
bool FillNumericSelectControl(int value,
                              FormFieldData* field) {
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    int option;
    if ((StringToInt(field->option_values[i], &option) && option == value) ||
        (StringToInt(field->option_contents[i], &option) && option == value)) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

bool FillStateSelectControl(const base::string16& value,
                            FormFieldData* field) {
  base::string16 full, abbreviation;
  state_names::GetNameAndAbbreviation(value, &full, &abbreviation);

  // Try an exact match of the abbreviation first.
  if (!abbreviation.empty() && SetSelectControlValue(abbreviation, field)) {
    return true;
  }

  // Try an exact match of the full name.
  if (!full.empty() && SetSelectControlValue(full, field)) {
    return true;
  }

  // Then try an inexact match of the full name.
  if (!full.empty() && SetSelectControlValueSubstringMatch(full, field)) {
    return true;
  }

  // Then try an inexact match of the abbreviation name.
  return !abbreviation.empty() &&
      SetSelectControlValueTokenMatch(abbreviation, field);
}

bool FillCountrySelectControl(const base::string16& value,
                              const std::string& app_locale,
                              FormFieldData* field_data) {
  std::string country_code = AutofillCountry::GetCountryCode(value, app_locale);
  if (country_code.empty())
    return false;

  DCHECK_EQ(field_data->option_values.size(),
            field_data->option_contents.size());
  for (size_t i = 0; i < field_data->option_values.size(); ++i) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    base::string16 value = field_data->option_values[i];
    base::string16 contents = field_data->option_contents[i];
    if (country_code == AutofillCountry::GetCountryCode(value, app_locale) ||
        country_code == AutofillCountry::GetCountryCode(contents, app_locale)) {
      field_data->value = value;
      return true;
    }
  }

  return false;
}

bool FillExpirationMonthSelectControl(const base::string16& value,
                                      FormFieldData* field) {
  int index = 0;
  if (!StringToInt(value, &index) ||
      index <= 0 ||
      static_cast<size_t>(index) >= arraysize(kMonthsFull))
    return false;

  bool filled =
      SetSelectControlValue(ASCIIToUTF16(kMonthsAbbreviated[index]), field) ||
      SetSelectControlValue(ASCIIToUTF16(kMonthsFull[index]), field) ||
      FillNumericSelectControl(index, field);
  return filled;
}

// Returns true if the last two digits in |year| match those in |str|.
bool LastTwoDigitsMatch(const base::string16& year,
                        const base::string16& str) {
  int year_int;
  int str_int;
  if (!StringToInt(year, &year_int) || !StringToInt(str, &str_int))
    return false;

  return (year_int % 100) == (str_int % 100);
}

// Try to fill a year |value| into the given |field| by comparing the last two
// digits of the year to the field's options.
bool FillYearSelectControl(const base::string16& value,
                           FormFieldData* field) {
  if (value.size() != 2U && value.size() != 4U)
    return false;

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (LastTwoDigitsMatch(value, field->option_values[i]) ||
        LastTwoDigitsMatch(value, field->option_contents[i])) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

// Try to fill a credit card type |value| (Visa, MasterCard, etc.) into the
// given |field|.
bool FillCreditCardTypeSelectControl(const base::string16& value,
                                     FormFieldData* field) {
  // Try stripping off spaces.
  base::string16 value_stripped;
  base::RemoveChars(base::StringToLowerASCII(value), base::kWhitespaceUTF16,
                    &value_stripped);

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    base::string16 option_value_lowercase;
    base::RemoveChars(base::StringToLowerASCII(field->option_values[i]),
                      base::kWhitespaceUTF16, &option_value_lowercase);
    base::string16 option_contents_lowercase;
    base::RemoveChars(base::StringToLowerASCII(field->option_contents[i]),
                      base::kWhitespaceUTF16, &option_contents_lowercase);

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

// Set |field_data|'s value to |number|, or possibly an appropriate substring of
// |number|.  The |field| specifies the type of the phone and whether this is a
// phone prefix or suffix.
void FillPhoneNumberField(const AutofillField& field,
                          const base::string16& number,
                          FormFieldData* field_data) {
  // Check to see if the size field matches the "prefix" or "suffix" sizes and
  // fill accordingly.
  base::string16 value = number;
  if (number.length() ==
          PhoneNumber::kPrefixLength + PhoneNumber::kSuffixLength) {
    if (field.phone_part() == AutofillField::PHONE_PREFIX ||
        field_data->max_length == PhoneNumber::kPrefixLength) {
      value = number.substr(PhoneNumber::kPrefixOffset,
                            PhoneNumber::kPrefixLength);
    } else if (field.phone_part() == AutofillField::PHONE_SUFFIX ||
               field_data->max_length == PhoneNumber::kSuffixLength) {
      value = number.substr(PhoneNumber::kSuffixOffset,
                            PhoneNumber::kSuffixLength);
    }
  }

  field_data->value = value;
}

// Fills in the select control |field| with |value|.  If an exact match is not
// found, falls back to alternate filling strategies based on the |type|.
bool FillSelectControl(const AutofillType& type,
                       const base::string16& value,
                       const std::string& app_locale,
                       FormFieldData* field) {
  DCHECK_EQ("select-one", field->form_control_type);

  // Guard against corrupted values passed over IPC.
  if (field->option_values.size() != field->option_contents.size())
    return false;

  if (value.empty())
    return false;

  // First, search for exact matches.
  if (SetSelectControlValue(value, field))
    return true;

  // If that fails, try specific fallbacks based on the field type.
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_STATE) {
    return FillStateSelectControl(value, field);
  } else if (storable_type == ADDRESS_HOME_COUNTRY) {
    return FillCountrySelectControl(value, app_locale, field);
  } else if (storable_type == CREDIT_CARD_EXP_MONTH) {
    return FillExpirationMonthSelectControl(value, field);
  } else if (storable_type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
             storable_type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    return FillYearSelectControl(value, field);
  } else if (storable_type == CREDIT_CARD_TYPE) {
    return FillCreditCardTypeSelectControl(value, field);
  }

  return false;
}

// Fills in the month control |field| with |value|.  |value| should be a date
// formatted as MM/YYYY.  If it isn't, filling will fail.
bool FillMonthControl(const base::string16& value, FormFieldData* field) {
  // Autofill formats a combined date as month/year.
  std::vector<base::string16> pieces;
  base::SplitString(value, base::char16('/'), &pieces);
  if (pieces.size() != 2)
    return false;

  // HTML5 input="month" is formatted as year-month.
  base::string16 month = pieces[0];
  base::string16 year = pieces[1];
  if ((month.size() != 1 && month.size() != 2) || year.size() != 4)
    return false;

  // HTML5 input="month" expects zero-padded months.
  if (month.size() == 1)
    month = ASCIIToUTF16("0") + month;

  field->value = year + ASCIIToUTF16("-") + month;
  return true;
}

// Fills |field| with the street address in |value|. Translates newlines into
// equivalent separators when necessary, i.e. when filling a single-line field.
// The separators depend on |address_language_code|.
void FillStreetAddress(const base::string16& value,
                       const std::string& address_language_code,
                       FormFieldData* field) {
  if (field->form_control_type == "textarea") {
    field->value = value;
    return;
  }

  AddressData address_data;
  address_data.language_code = address_language_code;
  base::SplitString(base::UTF16ToUTF8(value), '\n', &address_data.address_line);
  std::string line;
  GetStreetAddressLinesAsSingleLine(address_data, &line);
  field->value = base::UTF8ToUTF16(line);
}

std::string Hash32Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(20U, hash_bin.length());

  uint32 hash32 = ((hash_bin[0] & 0xFF) << 24) |
                  ((hash_bin[1] & 0xFF) << 16) |
                  ((hash_bin[2] & 0xFF) << 8) |
                   (hash_bin[3] & 0xFF);

  return base::UintToString(hash32);
}

}  // namespace

AutofillField::AutofillField()
    : server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNKNOWN),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED) {
}

AutofillField::AutofillField(const FormFieldData& field,
                             const base::string16& unique_name)
    : FormFieldData(field),
      unique_name_(unique_name),
      server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNKNOWN),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED) {
}

AutofillField::~AutofillField() {}

void AutofillField::set_heuristic_type(ServerFieldType type) {
  if (type >= 0 && type < MAX_VALID_FIELD_TYPE &&
      type != FIELD_WITH_DEFAULT_VALUE) {
    heuristic_type_ = type;
  } else {
    NOTREACHED();
    // This case should not be reachable; but since this has potential
    // implications on data uploaded to the server, better safe than sorry.
    heuristic_type_ = UNKNOWN_TYPE;
  }
}

void AutofillField::set_server_type(ServerFieldType type) {
  // Chrome no longer supports fax numbers, but the server still does.
  if (type >= PHONE_FAX_NUMBER && type <= PHONE_FAX_WHOLE_NUMBER)
    return;

  server_type_ = type;
}

void AutofillField::SetHtmlType(HtmlFieldType type, HtmlFieldMode mode) {
  html_type_ = type;
  html_mode_ = mode;

  if (type == HTML_TYPE_TEL_LOCAL_PREFIX)
    phone_part_ = PHONE_PREFIX;
  else if (type == HTML_TYPE_TEL_LOCAL_SUFFIX)
    phone_part_ = PHONE_SUFFIX;
  else
    phone_part_ = IGNORED;
}

AutofillType AutofillField::Type() const {
  if (html_type_ != HTML_TYPE_UNKNOWN)
    return AutofillType(html_type_, html_mode_);

  if (server_type_ != NO_SERVER_DATA)
    return AutofillType(server_type_);

  return AutofillType(heuristic_type_);
}

bool AutofillField::IsEmpty() const {
  return value.empty();
}

std::string AutofillField::FieldSignature() const {
  std::string field_name = base::UTF16ToUTF8(name);
  std::string field_string = field_name + "&" + form_control_type;
  return Hash32Bit(field_string);
}

bool AutofillField::IsFieldFillable() const {
  return should_autocomplete && !Type().IsUnknown();
}

// static
bool AutofillField::FillFormField(const AutofillField& field,
                                  const base::string16& value,
                                  const std::string& address_language_code,
                                  const std::string& app_locale,
                                  FormFieldData* field_data) {
  AutofillType type = field.Type();

  if (type.GetStorableType() == PHONE_HOME_NUMBER) {
    FillPhoneNumberField(field, value, field_data);
    return true;
  } else if (field_data->form_control_type == "select-one") {
    return FillSelectControl(type, value, app_locale, field_data);
  } else if (field_data->form_control_type == "month") {
    return FillMonthControl(value, field_data);
  } else if (type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    FillStreetAddress(value, address_language_code, field_data);
    return true;
  }

  field_data->value = value;
  return true;
}

}  // namespace autofill
