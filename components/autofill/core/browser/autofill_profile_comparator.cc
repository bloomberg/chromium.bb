// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_comparator.h"

#include <algorithm>
#include <vector>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace autofill {
namespace {

const base::char16 kSpace[] = {L' ', L'\0'};

}  // namespace

AutofillProfileComparator::AutofillProfileComparator(
    const base::StringPiece& app_locale)
    : app_locale_(app_locale.data(), app_locale.size()) {
  // Use ICU transliteration to remove diacritics and fold case.
  // See http://userguide.icu-project.org/transforms/general
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance(
          "NFD; [:Nonspacing Mark:] Remove; Lower; NFC", UTRANS_FORWARD,
          status));
  if (U_FAILURE(status) || transliterator == nullptr) {
    // TODO(rogerm): Add a histogram to count how often this happens.
    LOG(ERROR) << "Failed to create ICU Transliterator: "
               << u_errorName(status);
  }

  transliterator_ = std::move(transliterator);
}

AutofillProfileComparator::~AutofillProfileComparator() {}

base::string16 AutofillProfileComparator::NormalizeForComparison(
    base::StringPiece16 text,
    AutofillProfileComparator::WhitespaceSpec whitespace_spec) const {
  // This algorithm is not designed to be perfect, we could get arbitrarily
  // fancy here trying to canonicalize address lines. Instead, this is designed
  // to handle common cases for all types of data (addresses and names) without
  // the need of domain-specific logic.
  //
  // 1. Convert punctuation to spaces and normalize all whitespace to spaces.
  //    This will convert "Mid-Island Plz." -> "Mid Island Plz " (the trailing
  //    space will be trimmed off outside of the end of the loop).
  //
  // 2. Collapse consecutive punctuation/whitespace characters to a single
  //    space. We pretend the string has already started with whitespace in
  //    order to trim leading spaces.
  //
  // 3. Remove diacritics (accents and other non-spacing marks) and perform
  //    case folding to lower-case.
  base::string16 result;
  result.reserve(text.length());
  bool previous_was_whitespace = (whitespace_spec == RETAIN_WHITESPACE);
  for (base::i18n::UTF16CharIterator iter(text.data(), text.length());
       !iter.end(); iter.Advance()) {
    switch (u_charType(iter.get())) {
      // Punctuation
      case U_DASH_PUNCTUATION:
      case U_START_PUNCTUATION:
      case U_END_PUNCTUATION:
      case U_CONNECTOR_PUNCTUATION:
      case U_OTHER_PUNCTUATION:
      // Whitespace
      case U_CONTROL_CHAR:  // To escape the '\n' character.
      case U_SPACE_SEPARATOR:
      case U_LINE_SEPARATOR:
      case U_PARAGRAPH_SEPARATOR:
        if (!previous_was_whitespace && whitespace_spec == RETAIN_WHITESPACE) {
          result.push_back(' ');
          previous_was_whitespace = true;
        }
        break;

      default:
        previous_was_whitespace = false;
        base::WriteUnicodeCharacter(iter.get(), &result);
        break;
    }
  }

  // Trim off trailing whitespace if we left one.
  if (previous_was_whitespace && !result.empty())
    result.resize(result.size() - 1);

  if (transliterator_ == nullptr)
    return result;

  icu::UnicodeString value = icu::UnicodeString(result.data(), result.length());
  transliterator_->transliterate(value);
  return base::string16(value.getBuffer(), value.length());
}

bool AutofillProfileComparator::AreMergeable(const AutofillProfile& p1,
                                             const AutofillProfile& p2) const {
  // Sorted in order to relative expense of the tests to fail early and cheaply
  // if possible.
  return HaveMergeableEmailAddresses(p1, p2) &&
         HaveMergeableCompanyNames(p1, p2) &&
         HaveMergeablePhoneNumbers(p1, p2) && HaveMergeableNames(p1, p2) &&
         HaveMergeableAddresses(p1, p2);
}

// static
std::set<base::StringPiece16> AutofillProfileComparator::UniqueTokens(
    base::StringPiece16 s) {
  std::vector<base::StringPiece16> tokens = base::SplitStringPiece(
      s, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::set<base::StringPiece16>(tokens.begin(), tokens.end());
}

// static
bool AutofillProfileComparator::HaveSameTokens(base::StringPiece16 s1,
                                               base::StringPiece16 s2) {
  std::set<base::StringPiece16> t1 = UniqueTokens(s1);
  std::set<base::StringPiece16> t2 = UniqueTokens(s2);

  // Note: std::include() expects the items in each range to be in sorted order,
  // hence the use of std::set<> instead of std::unordered_set<>.
  return std::includes(t1.begin(), t1.end(), t2.begin(), t2.end()) ||
         std::includes(t2.begin(), t2.end(), t1.begin(), t1.end());
}

// static
std::set<base::string16> AutofillProfileComparator::GetNamePartVariants(
    const base::string16& name_part) {
  const size_t kMaxSupportedSubNames = 8;

  std::vector<base::string16> sub_names = base::SplitString(
      name_part, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Limit the number of sub-names we support (to constrain memory usage);
  if (sub_names.size() > kMaxSupportedSubNames)
    return {name_part};

  // Start with the empty string as a variant.
  std::set<base::string16> variants = {base::EmptyString16()};

  // For each sub-name, add a variant of all the already existing variants that
  // appends this sub-name and one that appends the initial of this sub-name.
  // Duplicates will be discarded when they're added to the variants set.
  for (const base::string16& sub_name : sub_names) {
    if (sub_name.empty()) continue;
    std::vector<base::string16> new_variants;
    for (const base::string16& variant : variants) {
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name}, kSpace), true));
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name.substr(0, 1)}, kSpace), true));
    }
    variants.insert(new_variants.begin(), new_variants.end());
  }

  // As a common case, also add the variant that just concatenates all of the
  // initials.
  base::string16 initials;
  for (const base::string16& sub_name : sub_names) {
    if (sub_name.empty()) continue;
    initials.push_back(sub_name[0]);
  }
  variants.insert(initials);

  // And, we're done.
  return variants;
}

bool AutofillProfileComparator::IsNameVariantOf(
    const base::string16& full_name_1,
    const base::string16& full_name_2) const {
  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  // Build the variants of full_name_1`s given, middle and family names.
  //
  // TODO(rogerm): Figure out whether or not we should break apart a compound
  // family name into variants (crbug/619051)
  const std::set<base::string16> given_name_variants =
      GetNamePartVariants(name_1_parts.given);
  const std::set<base::string16> middle_name_variants =
      GetNamePartVariants(name_1_parts.middle);
  const base::string16& family_name = name_1_parts.family;

  // Iterate over all full name variants of profile 2 and see if any of them
  // match the full name from profile 1.
  for (const base::string16& given_name : given_name_variants) {
    for (const base::string16& middle_name : middle_name_variants) {
      base::string16 candidate = base::CollapseWhitespace(
          base::JoinString({given_name, middle_name, family_name}, kSpace),
          true);
      if (candidate == full_name_2)
        return true;
    }
  }

  // Also check if the name is just composed of the user's initials. For
  // example, "thomas jefferson miller" could be composed as "tj miller".
  if (!name_1_parts.given.empty() && !name_1_parts.middle.empty()) {
    base::string16 initials;
    initials.push_back(name_1_parts.given[0]);
    initials.push_back(name_1_parts.middle[0]);
    base::string16 candidate = base::CollapseWhitespace(
        base::JoinString({initials, family_name}, kSpace), true);
    if (candidate == full_name_2)
      return true;
  }

  // There was no match found.
  return false;
}

bool AutofillProfileComparator::HaveMergeableNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  base::string16 full_name_1 =
      NormalizeForComparison(p1.GetInfo(AutofillType(NAME_FULL), app_locale_));
  base::string16 full_name_2 =
      NormalizeForComparison(p2.GetInfo(AutofillType(NAME_FULL), app_locale_));

  // Is it reasonable to merge the names from p1 and p2.
  return full_name_1.empty() || full_name_2.empty() ||
         (full_name_1 == full_name_2) ||
         IsNameVariantOf(full_name_1, full_name_2) ||
         IsNameVariantOf(full_name_2, full_name_1);
}

bool AutofillProfileComparator::HaveMergeableEmailAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const base::string16& email_1 =
      p1.GetInfo(AutofillType(EMAIL_ADDRESS), app_locale_);
  const base::string16& email_2 =
      p2.GetInfo(AutofillType(EMAIL_ADDRESS), app_locale_);
  return email_1.empty() || email_2.empty() ||
         case_insensitive_compare_.StringsEqual(email_1, email_2);
}

bool AutofillProfileComparator::HaveMergeableCompanyNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const base::string16& company_name_1 = NormalizeForComparison(
      p1.GetInfo(AutofillType(COMPANY_NAME), app_locale_));
  const base::string16& company_name_2 = NormalizeForComparison(
      p2.GetInfo(AutofillType(COMPANY_NAME), app_locale_));
  return company_name_1.empty() || company_name_2.empty() ||
         HaveSameTokens(company_name_1, company_name_2);
}

bool AutofillProfileComparator::HaveMergeablePhoneNumbers(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  // We work with the raw phone numbers to avoid losing any helpful information
  // as we parse.
  const base::string16& raw_phone_1 = p1.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);
  const base::string16& raw_phone_2 = p2.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);

  // Are the two phone numbers trivially mergeable?
  if (raw_phone_1.empty() || raw_phone_2.empty() ||
      raw_phone_1 == raw_phone_2) {
    return true;
  }

  // TODO(rogerm): Modify ::autofill::i18n::PhoneNumbersMatch to support
  // SHORT_NSN_MATCH and just call that instead of accessing the underlying
  // utility library directly?

  // The phone number util library needs the numbers in utf8.
  const std::string phone_1 = base::UTF16ToUTF8(raw_phone_1);
  const std::string phone_2 = base::UTF16ToUTF8(raw_phone_2);

  // Parse and compare the phone numbers.
  using ::i18n::phonenumbers::PhoneNumberUtil;
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  switch (phone_util->IsNumberMatchWithTwoStrings(phone_1, phone_2)) {
    case PhoneNumberUtil::INVALID_NUMBER:
    case PhoneNumberUtil::NO_MATCH:
      return false;
    case PhoneNumberUtil::SHORT_NSN_MATCH:
    case PhoneNumberUtil::NSN_MATCH:
    case PhoneNumberUtil::EXACT_MATCH:
      return true;
  }

  NOTREACHED();
  return false;
}

bool AutofillProfileComparator::HaveMergeableAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  // If the address are not in the same country, then they're not the same. If
  // one of the address countries is unknown/invalid the comparison continues.
  const base::string16& country1 = p1.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), app_locale_);
  const base::string16& country2 = p2.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), app_locale_);
  if (!country1.empty() && !country2.empty() &&
      !case_insensitive_compare_.StringsEqual(country1, country2)) {
    return false;
  }

  // TODO(rogerm): Lookup the normalization rules for the (common) country of
  // the address. The rules should be applied post NormalizeForComparison to
  // the state, city, and address bag of words comparisons.

  // Zip
  // ----
  // If the addresses are definitely not in the same zip/area code then we're
  // done. Otherwise,the comparison continues.
  const base::string16& zip1 = NormalizeForComparison(
      p1.GetInfo(AutofillType(ADDRESS_HOME_ZIP), app_locale_),
      DISCARD_WHITESPACE);
  const base::string16& zip2 = NormalizeForComparison(
      p2.GetInfo(AutofillType(ADDRESS_HOME_ZIP), app_locale_),
      DISCARD_WHITESPACE);
  if (!zip1.empty() && !zip2.empty() &&
      zip1.find(zip2) == base::string16::npos &&
      zip2.find(zip1) == base::string16::npos) {
    return false;
  }

  // State
  // ------
  // Heuristic: If the match is between non-empty zip codes then we can infer
  // that the two state strings are intended to have the same meaning. This
  // handles the cases where we have invalid or poorly formed data in one of the
  // state values (like "Select one", or "CA - California"). Otherwise, we
  // actually have to check if the states map to the the same set of tokens.
  const base::string16& state1 = NormalizeForComparison(
      p1.GetInfo(AutofillType(ADDRESS_HOME_STATE), app_locale_));
  const base::string16& state2 = NormalizeForComparison(
      p2.GetInfo(AutofillType(ADDRESS_HOME_STATE), app_locale_));
  if ((zip1.empty() || zip2.empty()) && !HaveSameTokens(state1, state2)) {
    return false;
  }

  // City
  // ------
  // Heuristic: If the match is between non-empty zip codes then we can infer
  // that the two city strings are intended to have the same meaning. This
  // handles the cases where we have a city vs one of its suburbs. Otherwise, we
  // actually have to check if the cities map to the the same set of tokens.
  const base::string16& city1 = NormalizeForComparison(
      p1.GetInfo(AutofillType(ADDRESS_HOME_CITY), app_locale_));
  const base::string16& city2 = NormalizeForComparison(
      p2.GetInfo(AutofillType(ADDRESS_HOME_CITY), app_locale_));
  if ((zip1.empty() || zip2.empty()) && !HaveSameTokens(city1, city2)) {
    return false;
  }

  // Address
  // --------
  // Heuristic: Use bag of words comparison on the post-normalized addresses.
  const base::string16& address1 = NormalizeForComparison(
      p1.GetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), app_locale_));
  const base::string16& address2 = NormalizeForComparison(
      p2.GetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), app_locale_));
  if (!HaveSameTokens(address1, address2)) {
    return false;
  }

  return true;
}

}  // namespace autofill
