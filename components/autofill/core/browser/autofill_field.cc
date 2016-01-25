// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/state_names.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
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

// Returns true if the value was successfully set, meaning |value| was found in
// the list of select options in |field|.
bool SetSelectControlValue(const base::string16& value,
                           FormFieldData* field) {
  l10n::CaseInsensitiveCompare compare;

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  base::string16 best_match;
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (value == field->option_values[i] ||
        value == field->option_contents[i]) {
      // An exact match, use it.
      best_match = field->option_values[i];
      break;
    }

    if (compare.StringsEqual(value, field->option_values[i]) ||
        compare.StringsEqual(value, field->option_contents[i])) {
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
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  int best_match = -1;

  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(value);
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (searcher.Search(field->option_values[i], nullptr, nullptr) ||
        searcher.Search(field->option_contents[i], nullptr, nullptr)) {
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
  std::vector<base::string16> tokenized;
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  l10n::CaseInsensitiveCompare compare;

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    tokenized = base::SplitString(
        field->option_values[i], base::kWhitespaceASCIIAs16,
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](base::string16& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
      field->value = field->option_values[i];
      return true;
    }

    tokenized = base::SplitString(
        field->option_contents[i], base::kWhitespaceASCIIAs16,
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](base::string16& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
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
                              FormFieldData* field_data) {
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty())
    return false;

  DCHECK_EQ(field_data->option_values.size(),
            field_data->option_contents.size());
  for (size_t i = 0; i < field_data->option_values.size(); ++i) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    base::string16 value = field_data->option_values[i];
    base::string16 contents = field_data->option_contents[i];
    if (country_code == CountryNames::GetInstance()->GetCountryCode(value) ||
        country_code == CountryNames::GetInstance()->GetCountryCode(contents)) {
      field_data->value = value;
      return true;
    }
  }

  return false;
}

bool FillExpirationMonthSelectControl(const base::string16& value,
                                      const std::string& app_locale,
                                      FormFieldData* field) {
  int index = 0;
  if (!StringToInt(value, &index) || index <= 0 || index > 12)
    return false;

  if (field->option_values.size() == 12) {
    // The select only contains the months.
    // If the first value of the select is 0, decrement the value of the index
    // so January is associated with 0 instead of 1.
    int first_value;
    if (StringToInt(field->option_values[0], &first_value) && first_value == 0)
      --index;
  } else if (field->option_values.size() == 13) {
    // The select uses the first value as a placeholder.
    // If the first value of the select is 1, increment the value of the index
    // to skip the placeholder value (January = 2).
    int first_value;
    if (StringToInt(field->option_values[0], &first_value) && first_value == 1)
      ++index;
  }

  for (const base::string16& option_value : field->option_values) {
    int converted_value = 0;
    if (CreditCard::ConvertMonth(option_value, app_locale, &converted_value) &&
        index == converted_value) {
      field->value = option_value;
      return true;
    }
  }

  for (const base::string16& option_contents : field->option_contents) {
    int converted_contents = 0;
    if (CreditCard::ConvertMonth(option_contents, app_locale,
                                 &converted_contents) &&
        index == converted_contents) {
      field->value = option_contents;
      return true;
    }
  }

  return FillNumericSelectControl(index, field);
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
  size_t idx;
  if (AutofillField::FindValueInSelectControl(*field, value, &idx)) {
    field->value = field->option_values[idx];
    return true;
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
  field_data->value =
      AutofillField::GetPhoneNumberValue(field, number, *field_data);
}

// Set |field_data|'s value to |number|, or possibly an appropriate substring
// of |number| for cases where credit card number splits across multiple HTML
// form input fields.
// The |field| specifies the |credit_card_number_offset_| to the substring
// within credit card number.
void FillCreditCardNumberField(const AutofillField& field,
                               const base::string16& number,
                               FormFieldData* field_data) {
  base::string16 value = number;

  // |field|'s max_length truncates credit card number to fit within.
  if (field.credit_card_number_offset() < value.length())
    value = value.substr(field.credit_card_number_offset());

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

  ServerFieldType storable_type = type.GetStorableType();

  // Credit card expiration month is checked first since an exact match on value
  // may not be correct.
  if (storable_type == CREDIT_CARD_EXP_MONTH)
    return FillExpirationMonthSelectControl(value, app_locale, field);

  // Search for exact matches.
  if (SetSelectControlValue(value, field))
    return true;

  // If that fails, try specific fallbacks based on the field type.
  if (storable_type == ADDRESS_HOME_STATE) {
    return FillStateSelectControl(value, field);
  } else if (storable_type == ADDRESS_HOME_COUNTRY) {
    return FillCountrySelectControl(value, field);
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
  std::vector<base::string16> pieces = base::SplitString(
      value, base::ASCIIToUTF16("/"),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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
  address_data.address_line = base::SplitString(
      base::UTF16ToUTF8(value), "\n",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string line;
  GetStreetAddressLinesAsSingleLine(address_data, &line);
  field->value = base::UTF8ToUTF16(line);
}

std::string Hash32Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(base::kSHA1Length, hash_bin.length());

  uint32_t hash32 = ((hash_bin[0] & 0xFF) << 24) |
                    ((hash_bin[1] & 0xFF) << 16) | ((hash_bin[2] & 0xFF) << 8) |
                    (hash_bin[3] & 0xFF);

  return base::UintToString(hash32);
}

base::string16 RemoveWhitespace(const base::string16& value) {
  base::string16 stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

}  // namespace

AutofillField::AutofillField()
    : server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNSPECIFIED),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED),
      credit_card_number_offset_(0),
      previously_autofilled_(false) {}

AutofillField::AutofillField(const FormFieldData& field,
                             const base::string16& unique_name)
    : FormFieldData(field),
      unique_name_(unique_name),
      server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNSPECIFIED),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED),
      credit_card_number_offset_(0),
      previously_autofilled_(false),
      parseable_name_(field.name) {}

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
  if (html_type_ != HTML_TYPE_UNSPECIFIED)
    return AutofillType(html_type_, html_mode_);

  if (server_type_ != NO_SERVER_DATA) {
    // See http://crbug.com/429236 for background on why we might not always
    // believe the server.
    // See http://crbug.com/441488 for potential improvements to the server
    // which may obviate the need for this logic.
    bool believe_server =
        !(server_type_ == NAME_FULL && heuristic_type_ == CREDIT_CARD_NAME) &&
        !(server_type_ == CREDIT_CARD_NAME && heuristic_type_ == NAME_FULL) &&
        // CVC is sometimes type="password", which tricks the server.
        // See http://crbug.com/469007
        !(AutofillType(server_type_).group() == PASSWORD_FIELD &&
              heuristic_type_ == CREDIT_CARD_VERIFICATION_CODE);
    if (believe_server)
      return AutofillType(server_type_);
  }

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
  return !Type().IsUnknown();
}

// static
bool AutofillField::FillFormField(const AutofillField& field,
                                  const base::string16& value,
                                  const std::string& address_language_code,
                                  const std::string& app_locale,
                                  FormFieldData* field_data) {
  AutofillType type = field.Type();

  // Don't fill if autocomplete=off is set on |field| on desktop for non credit
  // card related fields.
  if (!field.should_autocomplete && IsDesktopPlatform() &&
      (type.group() != CREDIT_CARD)) {
    return false;
  }

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
  } else if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    FillCreditCardNumberField(field, value, field_data);
    return true;
  }

  field_data->value = value;
  return true;
}

base::string16 AutofillField::GetPhoneNumberValue(
    const AutofillField& field,
    const base::string16& number,
    const FormFieldData& field_data) {
  // Check to see if the size field matches the "prefix" or "suffix" size.
  // If so, return the appropriate substring.
  if (number.length() !=
          PhoneNumber::kPrefixLength + PhoneNumber::kSuffixLength) {
    return number;
  }

  if (field.phone_part() == AutofillField::PHONE_PREFIX ||
      field_data.max_length == PhoneNumber::kPrefixLength) {
    return
        number.substr(PhoneNumber::kPrefixOffset, PhoneNumber::kPrefixLength);
  }

  if (field.phone_part() == AutofillField::PHONE_SUFFIX ||
      field_data.max_length == PhoneNumber::kSuffixLength) {
    return
        number.substr(PhoneNumber::kSuffixOffset, PhoneNumber::kSuffixLength);
  }

  return number;
}

// static
bool AutofillField::FindValueInSelectControl(const FormFieldData& field,
                                             const base::string16& value,
                                             size_t* index) {
  l10n::CaseInsensitiveCompare compare;
  // Strip off spaces for all values in the comparisons.
  const base::string16 value_stripped = RemoveWhitespace(value);

  for (size_t i = 0; i < field.option_values.size(); ++i) {
    base::string16 option_value = RemoveWhitespace(field.option_values[i]);
    if (compare.StringsEqual(value_stripped, option_value)) {
      if (index)
        *index = i;
      return true;
    }

    base::string16 option_contents = RemoveWhitespace(field.option_contents[i]);
    if (compare.StringsEqual(value_stripped, option_contents)) {
      if (index)
        *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace autofill
