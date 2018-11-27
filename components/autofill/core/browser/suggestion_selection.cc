// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/suggestion_selection.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {
namespace suggestion_selection {

namespace {
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

// In addition to just getting the values out of the autocomplete profile, this
// function handles formatting of the street address into a single string.
base::string16 GetInfoInOneLine(const AutofillProfile* profile,
                                const AutofillType& type,
                                const std::string& app_locale) {
  std::vector<base::string16> results;

  AddressField address_field;
  if (i18n::FieldForType(type.GetStorableType(), &address_field) &&
      address_field == STREET_ADDRESS) {
    std::string street_address_line;
    GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile->GetInfo(type, app_locale);
}
}  // namespace

// As of November 2018, 50 profiles should be more than enough to cover at least
// 99% of all times the dropdown is shown.
extern const size_t kMaxSuggestedProfilesCount = 50;

// As of November 2018, displaying 10 suggestions cover at least 99% of the
// indices clicked by our users. The suggestions will also refine as they type.
extern const size_t kMaxUniqueSuggestionsCount = 10;

std::vector<Suggestion> GetPrefixMatchedSuggestions(
    const AutofillType& type,
    const base::string16& field_contents_canon,
    const AutofillProfileComparator& comparator,
    const std::vector<AutofillProfile*>& profiles,
    std::vector<AutofillProfile*>* matched_profiles) {
  std::vector<Suggestion> suggestions;

  for (size_t i = 0; i < profiles.size() &&
                     matched_profiles->size() < kMaxSuggestedProfilesCount;
       i++) {
    AutofillProfile* profile = profiles[i];
    base::string16 value =
        GetInfoInOneLine(profile, type, comparator.app_locale());
    if (value.empty())
      continue;

    bool prefix_matched_suggestion;
    base::string16 suggestion_canon = comparator.NormalizeForComparison(value);
    if (IsValidSuggestionForFieldContents(
            suggestion_canon, field_contents_canon, type,
            /* is_masked_server_card= */ false, &prefix_matched_suggestion)) {
      matched_profiles->push_back(profile);
      suggestions.push_back(Suggestion(value));
      suggestions.back().backend_id = profile->guid();
      suggestions.back().match = prefix_matched_suggestion
                                     ? Suggestion::PREFIX_MATCH
                                     : Suggestion::SUBSTRING_MATCH;
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  return suggestions;
}

std::vector<Suggestion> GetUniqueSuggestions(
    const std::vector<ServerFieldType>& other_field_types,
    const std::string app_locale,
    const std::vector<AutofillProfile*> matched_profiles,
    const std::vector<Suggestion>& suggestions,
    std::vector<AutofillProfile*>* unique_matched_profiles) {
  std::vector<Suggestion> unique_suggestions;

  // Limit number of unique profiles as having too many makes the browser hang
  // due to drawing calculations (and is also not very useful for the user).
  ServerFieldTypeSet types(other_field_types.begin(), other_field_types.end());
  for (size_t i = 0; i < matched_profiles.size() &&
                     unique_suggestions.size() < kMaxUniqueSuggestionsCount;
       ++i) {
    bool include = true;
    AutofillProfile* profile_a = matched_profiles[i];
    for (size_t j = 0; j < matched_profiles.size(); ++j) {
      AutofillProfile* profile_b = matched_profiles[j];
      // Check if profile A is a subset of profile B. If not, continue.
      if (i == j || suggestions[i].value != suggestions[j].value ||
          !profile_a->IsSubsetOfForFieldSet(*profile_b, app_locale, types)) {
        continue;
      }

      // Check if profile B is also a subset of profile A. If so, the
      // profiles are identical. Include the first one but not the second.
      if (i < j &&
          profile_b->IsSubsetOfForFieldSet(*profile_a, app_locale, types)) {
        continue;
      }

      // One-way subset. Don't include profile A.
      include = false;
      break;
    }
    if (include) {
      unique_matched_profiles->push_back(matched_profiles[i]);
      unique_suggestions.push_back(suggestions[i]);
    }
  }

  return unique_suggestions;
}

bool IsValidSuggestionForFieldContents(base::string16 suggestion_canon,
                                       base::string16 field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool* is_prefix_matched) {
  *is_prefix_matched = true;

  // Phones should do a substring match because they can be trimmed to remove
  // the first parts (e.g. country code or prefix). It is still considered a
  // prefix match in order to put it at the top of the suggestions.
  if ((type.group() == PHONE_HOME || type.group() == PHONE_BILLING) &&
      suggestion_canon.find(field_contents_canon) != base::string16::npos) {
    return true;
  }

  // For card number fields, suggest the card if:
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewer typed so far.
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    if (suggestion_canon.find(field_contents_canon) == base::string16::npos &&
        (!is_masked_server_card || field_contents_canon.size() >= 6)) {
      return false;
    }
    return true;
  }

  if (base::StartsWith(suggestion_canon, field_contents_canon,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }

  if (IsFeatureSubstringMatchEnabled() &&
      suggestion_canon.length() >= field_contents_canon.length() &&
      GetTextSelectionStart(suggestion_canon, field_contents_canon, false) !=
          base::string16::npos) {
    *is_prefix_matched = false;
    return true;
  }

  return false;
}

void RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>* profiles) {
  const size_t original_size = profiles->size();
  profiles->erase(
      std::stable_partition(profiles->begin(), profiles->end(),
                            [min_last_used](const AutofillDataModel* m) {
                              return m->use_date() > min_last_used;
                            }),
      profiles->end());
  const size_t num_profiles_supressed = original_size - profiles->size();
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
      num_profiles_supressed);
}

}  // namespace suggestion_selection
}  // namespace autofill