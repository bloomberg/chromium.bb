// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/html_based_username_detector.h"

#include <algorithm>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector_vocabulary.h"
#include "third_party/WebKit/public/web/WebFormElement.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;

namespace autofill {

namespace {

// List of separators that can appear in HTML attribute values.
constexpr char kDelimiters[] = "$\"\'?%*@!\\/&^#:+~`;,>|<.[](){}-_ 0123456789";

// Minimum length of a word, in order not to be considered short word. Short
// words will not be searched in attribute values (especially after delimiters
// removing), because a short word may be a part of another word. A short word
// should be enclosed between delimiters, otherwise an occurrence doesn't count.
constexpr int kMinimumWordLength = 4;

// For each input element that can be a username, developer and user group
// values are computed. The user group value includes what a user sees: label,
// placeholder, aria-label (all are stored in FormFieldData.label). The
// developer group value consists of name and id attribute values.
// For each group the set of short tokens (tokens shorter than
// |kMinimumWordLength|) is computed as well.
struct UsernameFieldData {
  WebInputElement input_element;
  base::string16 developer_value;
  base::flat_set<base::string16> developer_short_tokens;
  base::string16 user_value;
  base::flat_set<base::string16> user_short_tokens;
};

// Words that the algorithm looks for are split into multiple categories based
// on feature reliability.
// A category may contain a latin dictionary and a non-latin dictionary. It is
// mandatory that it has a latin one, but a non-latin might be missing.
// "Latin" translations are the translations of the words for which the
// original translation is similar to the romanized translation (translation of
// the word only using ISO basic Latin alphabet).
// "Non-latin" translations are the translations of the words that have custom,
// country specific characters.
struct CategoryOfWords {
  const char* const* const latin_dictionary;
  const size_t latin_dictionary_size;
  const char* const* const non_latin_dictionary;
  const size_t non_latin_dictionary_size;
};

// 1. Removes delimiters from |raw_value| and appends it to |*field_data_value|.
// A sentinel symbol is added first if |*field_data_value| is not empty.
// 2. Tokenizes and appends short tokens (shorter than |kMinimumWordLength|)
// from |raw_value| to |*field_data_short_tokens|, if any.
void AppendValueAndShortTokens(
    const base::string16& raw_value,
    base::string16* field_data_value,
    base::flat_set<base::string16>* field_data_short_tokens) {
  base::string16 lowercase_value = base::i18n::ToLower(raw_value);
  const base::string16 delimiters = base::ASCIIToUTF16(kDelimiters);
  std::vector<base::StringPiece16> tokens =
      base::SplitStringPiece(lowercase_value, delimiters, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  // Modify |lowercase_value| only when |tokens| has been processed.

  std::vector<base::string16> short_tokens;
  std::transform(
      std::find_if(tokens.begin(), tokens.end(),
                   [](const base::StringPiece16& token) {
                     return token.size() < kMinimumWordLength;
                   }),
      tokens.end(), std::back_inserter(short_tokens),
      [](const base::StringPiece16& token) { return token.as_string(); });
  // It is better to insert elements to a |flat_map| in one operation.
  field_data_short_tokens->insert(short_tokens.begin(), short_tokens.end());

  // Now that tokens are processed, squeeze delimiters out of |lowercase_value|.
  lowercase_value.erase(std::remove_if(
      lowercase_value.begin(), lowercase_value.end(),
      [delimiters](char c) { return delimiters.find(c) != delimiters.npos; }));

  // When computing the developer value, '$' safety guard is being added
  // between field name and id, so that forming of accidental words is
  // prevented.
  if (!field_data_value->empty())
    field_data_value->push_back('$');
  *field_data_value += lowercase_value;
}

// For the given |input_element|, compute developer and user value, along with
// sets of short tokens, and returns it.
UsernameFieldData ComputeUsernameFieldData(
    const blink::WebInputElement& input_element,
    const FormFieldData& field) {
  UsernameFieldData field_data;
  field_data.input_element = input_element;

  AppendValueAndShortTokens(field.name, &field_data.developer_value,
                            &field_data.developer_short_tokens);
  AppendValueAndShortTokens(field.id, &field_data.developer_value,
                            &field_data.developer_short_tokens);
  AppendValueAndShortTokens(field.label, &field_data.user_value,
                            &field_data.user_short_tokens);
  return field_data;
}

// For the fields of the given form that can be username fields
// (all_possible_usernames), computes |UsernameFieldData| needed by the
// detector.
void InferUsernameFieldData(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    std::vector<UsernameFieldData>* possible_usernames_data) {
  // |all_possible_usernames| and |form_data.fields| may have different set of
  // fields. Match them based on |WebInputElement.NameForAutofill| and
  // |FormFieldData.name|.
  size_t next_element_range_begin = 0;

  for (const blink::WebInputElement& input_element : all_possible_usernames) {
    const base::string16 element_name = input_element.NameForAutofill().Utf16();
    for (size_t i = next_element_range_begin; i < form_data.fields.size();
         ++i) {
      const FormFieldData& field_data = form_data.fields[i];
      if (input_element.NameForAutofill().IsEmpty())
        continue;

      // Find matching field data and web input element.
      if (field_data.name == element_name) {
        next_element_range_begin = i + 1;
        possible_usernames_data->push_back(
            ComputeUsernameFieldData(input_element, field_data));
        break;
      }
    }
  }
}

// Check if any word from |dictionary| is encountered in computed field
// information (i.e. |value|, |tokens|).
bool CheckFieldWithDictionary(
    const base::string16& value,
    const base::flat_set<base::string16>& short_tokens,
    const char* const* dictionary,
    const size_t& dictionary_size) {
  for (size_t i = 0; i < dictionary_size; ++i) {
    const base::string16 word = base::UTF8ToUTF16(dictionary[i]);
    if (word.length() < kMinimumWordLength) {
      // Treat short words by looking them up in the tokens set.
      if (short_tokens.find(word) != short_tokens.end())
        return true;
    } else {
      // Treat long words by looking them up as a substring in |value|.
      if (value.find(word) != std::string::npos)
        return true;
    }
  }
  return false;
}

// Check if any word from |category| is encountered in computed field
// information (|possible_username|).
bool ContainsWordFromCategory(const UsernameFieldData& possible_username,
                              const CategoryOfWords& category) {
  // For user value, search in latin and non-latin dictionaries, because this
  // value is user visible. For developer value, only look up in latin
  /// dictionaries.
  return CheckFieldWithDictionary(
             possible_username.user_value, possible_username.user_short_tokens,
             category.latin_dictionary, category.latin_dictionary_size) ||
         CheckFieldWithDictionary(possible_username.user_value,
                                  possible_username.user_short_tokens,
                                  category.non_latin_dictionary,
                                  category.non_latin_dictionary_size) ||
         CheckFieldWithDictionary(possible_username.developer_value,
                                  possible_username.developer_short_tokens,
                                  category.latin_dictionary,
                                  category.latin_dictionary_size);
}

// Remove from |possible_usernames_data| the elements that definitely cannot be
// usernames, because their computed values contain at least one negative word.
void RemoveFieldsWithNegativeWords(
    std::vector<UsernameFieldData>* possible_usernames_data) {
  static const CategoryOfWords kNegativeCategory = {
      kNegativeLatin, kNegativeLatinSize, kNegativeNonLatin,
      kNegativeNonLatinSize};

  possible_usernames_data->erase(
      std::remove_if(possible_usernames_data->begin(),
                     possible_usernames_data->end(),
                     [](const UsernameFieldData& possible_username) {
                       return ContainsWordFromCategory(possible_username,
                                                       kNegativeCategory);
                     }),
      possible_usernames_data->end());
}

// Check if any word from the given category (|category|) appears in fields from
// the form (|possible_usernames_data|). If the category words appear in more
// than 2 fields, do not make a decision, because it may just be a prefix. If
// the words appears in 1 or 2 fields, the first field is saved to
// |*username_element|.
bool FormContainsWordFromCategory(
    const std::vector<UsernameFieldData>& possible_usernames_data,
    const CategoryOfWords& category,
    WebInputElement* username_element) {
  // Auxiliary element that contains the first field (in order of appearance in
  // the form) in which a substring is encountered.
  WebInputElement chosen_field;

  size_t fields_found = 0;
  for (const UsernameFieldData& field_data : possible_usernames_data) {
    if (ContainsWordFromCategory(field_data, category)) {
      if (fields_found == 0)
        chosen_field = field_data.input_element;
      fields_found++;
    }
  }

  if (fields_found > 0 && fields_found <= 2) {
    *username_element = chosen_field;
    return true;
  } else {
    return false;
  }
}

// Find username element if there is no cached result for the given form.
bool FindUsernameFieldInternal(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    WebInputElement* username_element) {
  DCHECK(username_element);

  static const CategoryOfWords kUsernameCategory = {
      kUsernameLatin, kUsernameLatinSize, kUsernameNonLatin,
      kUsernameNonLatinSize};
  static const CategoryOfWords kUserCategory = {
      kUserLatin, kUserLatinSize, kUserNonLatin, kUserNonLatinSize};
  static const CategoryOfWords kTechnicalCategory = {
      kTechnicalWords, kTechnicalWordsSize, nullptr, 0};
  static const CategoryOfWords kWeakCategory = {kWeakWords, kWeakWordsSize,
                                                nullptr, 0};
  // These categories contain words that point to username field.
  // Order of categories is vital: the detector searches for words in descending
  // order of probability to point to a username field.
  static const CategoryOfWords kPositiveCategories[] = {
      kUsernameCategory, kUserCategory, kTechnicalCategory, kWeakCategory};

  std::vector<UsernameFieldData> possible_usernames_data;
  InferUsernameFieldData(all_possible_usernames, form_data,
                         &possible_usernames_data);
  RemoveFieldsWithNegativeWords(&possible_usernames_data);

  // These are the searches performed by the username detector.
  for (const CategoryOfWords& category : kPositiveCategories) {
    if (FormContainsWordFromCategory(possible_usernames_data, category,
                                     username_element)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool GetUsernameFieldBasedOnHtmlAttributes(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    WebInputElement* username_element,
    UsernameDetectorCache* username_detector_cache) {
  DCHECK(username_element);

  if (all_possible_usernames.empty())
    return false;

  // All elements in |all_possible_usernames| should have the same |Form()|.
  DCHECK(
      std::adjacent_find(
          all_possible_usernames.begin(), all_possible_usernames.end(),
          [](const blink::WebInputElement& a, const blink::WebInputElement& b) {
            return a.Form() != b.Form();
          }) == all_possible_usernames.end());
  const blink::WebFormElement form = all_possible_usernames[0].Form();

  // True if the cache has no entry for |form|.
  bool cache_miss = true;
  // Iterator pointing to the entry for |form| if the entry for |form| is found.
  UsernameDetectorCache::iterator form_position;
  if (username_detector_cache) {
    std::tie(form_position, cache_miss) = username_detector_cache->insert(
        std::make_pair(form, blink::WebInputElement()));
  }

  if (!username_detector_cache || cache_miss) {
    bool username_found = FindUsernameFieldInternal(
        all_possible_usernames, form_data, username_element);
    if (username_detector_cache && username_found)
      form_position->second = *username_element;
    return username_found;
  } else {
    *username_element = form_position->second;
    return !username_element->IsNull();
  }
}

}  // namespace autofill
