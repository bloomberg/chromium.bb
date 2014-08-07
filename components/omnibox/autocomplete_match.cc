// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/autocomplete_match.h"

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/omnibox/autocomplete_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "grit/component_scaled_resources.h"

namespace {

bool IsTrivialClassification(const ACMatchClassifications& classifications) {
  return classifications.empty() ||
      ((classifications.size() == 1) &&
       (classifications.back().style == ACMatchClassification::NONE));
}

}  // namespace

// AutocompleteMatch ----------------------------------------------------------

// static
const base::char16 AutocompleteMatch::kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

AutocompleteMatch::AutocompleteMatch()
    : provider(NULL),
      relevance(0),
      typed_count(-1),
      deletable(false),
      allowed_to_be_default_match(false),
      transition(content::PAGE_TRANSITION_GENERATED),
      is_history_what_you_typed_match(false),
      type(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED),
      from_previous(false) {
}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type)
    : provider(provider),
      relevance(relevance),
      typed_count(-1),
      deletable(deletable),
      allowed_to_be_default_match(false),
      transition(content::PAGE_TRANSITION_TYPED),
      is_history_what_you_typed_match(false),
      type(type),
      from_previous(false) {
}

AutocompleteMatch::AutocompleteMatch(const AutocompleteMatch& match)
    : provider(match.provider),
      relevance(match.relevance),
      typed_count(match.typed_count),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      inline_autocompletion(match.inline_autocompletion),
      allowed_to_be_default_match(match.allowed_to_be_default_match),
      destination_url(match.destination_url),
      stripped_destination_url(match.stripped_destination_url),
      contents(match.contents),
      contents_class(match.contents_class),
      description(match.description),
      description_class(match.description_class),
      answer_contents(match.answer_contents),
      answer_type(match.answer_type),
      transition(match.transition),
      is_history_what_you_typed_match(match.is_history_what_you_typed_match),
      type(match.type),
      associated_keyword(match.associated_keyword.get() ?
          new AutocompleteMatch(*match.associated_keyword) : NULL),
      keyword(match.keyword),
      from_previous(match.from_previous),
      search_terms_args(match.search_terms_args.get() ?
          new TemplateURLRef::SearchTermsArgs(*match.search_terms_args) :
          NULL),
      additional_info(match.additional_info),
      duplicate_matches(match.duplicate_matches) {
}

AutocompleteMatch::~AutocompleteMatch() {
}

AutocompleteMatch& AutocompleteMatch::operator=(
    const AutocompleteMatch& match) {
  if (this == &match)
    return *this;

  provider = match.provider;
  relevance = match.relevance;
  typed_count = match.typed_count;
  deletable = match.deletable;
  fill_into_edit = match.fill_into_edit;
  inline_autocompletion = match.inline_autocompletion;
  allowed_to_be_default_match = match.allowed_to_be_default_match;
  destination_url = match.destination_url;
  stripped_destination_url = match.stripped_destination_url;
  contents = match.contents;
  contents_class = match.contents_class;
  description = match.description;
  description_class = match.description_class;
  answer_contents = match.answer_contents;
  answer_type = match.answer_type;
  transition = match.transition;
  is_history_what_you_typed_match = match.is_history_what_you_typed_match;
  type = match.type;
  associated_keyword.reset(match.associated_keyword.get() ?
      new AutocompleteMatch(*match.associated_keyword) : NULL);
  keyword = match.keyword;
  from_previous = match.from_previous;
  search_terms_args.reset(match.search_terms_args.get() ?
      new TemplateURLRef::SearchTermsArgs(*match.search_terms_args) : NULL);
  additional_info = match.additional_info;
  duplicate_matches = match.duplicate_matches;
  return *this;
}

// static
int AutocompleteMatch::TypeToIcon(Type type) {
  int icons[] = {
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_EXTENSION_APP,
    IDR_OMNIBOX_SEARCH,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_HTTP,
    IDR_OMNIBOX_SEARCH,
  };
  COMPILE_ASSERT(arraysize(icons) == AutocompleteMatchType::NUM_TYPES,
                 icons_array_must_match_type_enum);
  return icons[type];
}

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& elem1,
                                     const AutocompleteMatch& elem2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  return (elem1.relevance == elem2.relevance) ?
      (elem1.contents < elem2.contents) : (elem1.relevance > elem2.relevance);
}

// static
bool AutocompleteMatch::DestinationsEqual(const AutocompleteMatch& elem1,
                                          const AutocompleteMatch& elem2) {
  if (elem1.stripped_destination_url.is_empty() &&
      elem2.stripped_destination_url.is_empty())
    return false;
  return elem1.stripped_destination_url == elem2.stripped_destination_url;
}

// static
void AutocompleteMatch::ClassifyMatchInString(
    const base::string16& find_text,
    const base::string16& text,
    int style,
    ACMatchClassifications* classification) {
  ClassifyLocationInString(text.find(find_text), find_text.length(),
                           text.length(), style, classification);
}

// static
void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classification) {
  classification->clear();

  // Don't classify anything about an empty string
  // (AutocompleteMatch::Validate() checks this).
  if (overall_length == 0)
    return;

  // Mark pre-match portion of string (if any).
  if (match_location != 0) {
    classification->push_back(ACMatchClassification(0, style));
  }

  // Mark matching portion of string.
  if (match_location == base::string16::npos) {
    // No match, above classification will suffice for whole string.
    return;
  }
  // Classifying an empty match makes no sense and will lead to validation
  // errors later.
  DCHECK_GT(match_length, 0U);
  classification->push_back(ACMatchClassification(match_location,
      (style | ACMatchClassification::MATCH) & ~ACMatchClassification::DIM));

  // Mark post-match portion of string (if any).
  const size_t after_match(match_location + match_length);
  if (after_match < overall_length) {
    classification->push_back(ACMatchClassification(after_match, style));
  }
}

// static
AutocompleteMatch::ACMatchClassifications
    AutocompleteMatch::MergeClassifications(
    const ACMatchClassifications& classifications1,
    const ACMatchClassifications& classifications2) {
  // We must return the empty vector only if both inputs are truly empty.
  // The result of merging an empty vector with a single (0, NONE)
  // classification is the latter one-entry vector.
  if (IsTrivialClassification(classifications1))
    return classifications2.empty() ? classifications1 : classifications2;
  if (IsTrivialClassification(classifications2))
    return classifications1;

  ACMatchClassifications output;
  for (ACMatchClassifications::const_iterator i = classifications1.begin(),
       j = classifications2.begin(); i != classifications1.end();) {
    AutocompleteMatch::AddLastClassificationIfNecessary(&output,
        std::max(i->offset, j->offset), i->style | j->style);
    const size_t next_i_offset = (i + 1) == classifications1.end() ?
        static_cast<size_t>(-1) : (i + 1)->offset;
    const size_t next_j_offset = (j + 1) == classifications2.end() ?
        static_cast<size_t>(-1) : (j + 1)->offset;
    if (next_i_offset >= next_j_offset)
      ++j;
    if (next_j_offset >= next_i_offset)
      ++i;
  }

  return output;
}

// static
std::string AutocompleteMatch::ClassificationsToString(
    const ACMatchClassifications& classifications) {
  std::string serialized_classifications;
  for (size_t i = 0; i < classifications.size(); ++i) {
    if (i)
      serialized_classifications += ',';
    serialized_classifications += base::IntToString(classifications[i].offset) +
        ',' + base::IntToString(classifications[i].style);
  }
  return serialized_classifications;
}

// static
ACMatchClassifications AutocompleteMatch::ClassificationsFromString(
    const std::string& serialized_classifications) {
  ACMatchClassifications classifications;
  std::vector<std::string> tokens;
  Tokenize(serialized_classifications, ",", &tokens);
  DCHECK(!(tokens.size() & 1));  // The number of tokens should be even.
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int classification_offset = 0;
    int classification_style = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &classification_offset) ||
        !base::StringToInt(tokens[i + 1], &classification_style)) {
      NOTREACHED();
      return classifications;
    }
    classifications.push_back(ACMatchClassification(classification_offset,
                                                    classification_style));
  }
  return classifications;
}

// static
void AutocompleteMatch::AddLastClassificationIfNecessary(
    ACMatchClassifications* classifications,
    size_t offset,
    int style) {
  DCHECK(classifications);
  if (classifications->empty() || classifications->back().style != style) {
    DCHECK(classifications->empty() ||
           (offset > classifications->back().offset));
    classifications->push_back(ACMatchClassification(offset, style));
  }
}

// static
base::string16 AutocompleteMatch::SanitizeString(const base::string16& text) {
  // NOTE: This logic is mirrored by |sanitizeString()| in
  // omnibox_custom_bindings.js.
  base::string16 result;
  base::TrimWhitespace(text, base::TRIM_LEADING, &result);
  base::RemoveChars(result, kInvalidChars, &result);
  return result;
}

// static
bool AutocompleteMatch::IsSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_HISTORY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE ||
         IsSpecializedSearchType(type);
}

// static
bool AutocompleteMatch::IsSpecializedSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_ANSWER;
}

// static
TemplateURL* AutocompleteMatch::GetTemplateURLWithKeyword(
    TemplateURLService* template_url_service,
    const base::string16& keyword,
    const std::string& host) {
  if (template_url_service == NULL)
    return NULL;
  TemplateURL* template_url = keyword.empty() ?
      NULL : template_url_service->GetTemplateURLForKeyword(keyword);
  return (template_url || host.empty()) ?
      template_url : template_url_service->GetTemplateURLForHost(host);
}

// static
GURL AutocompleteMatch::GURLToStrippedGURL(
    const GURL& url,
    TemplateURLService* template_url_service,
    const base::string16& keyword) {
  if (!url.is_valid())
    return url;

  GURL stripped_destination_url = url;

  // If the destination URL looks like it was generated from a TemplateURL,
  // remove all substitutions other than the search terms.  This allows us
  // to eliminate cases like past search URLs from history that differ only
  // by some obscure query param from each other or from the search/keyword
  // provider matches.
  TemplateURL* template_url = GetTemplateURLWithKeyword(
      template_url_service, keyword, stripped_destination_url.host());
  if (template_url != NULL &&
      template_url->SupportsReplacement(
          template_url_service->search_terms_data())) {
    base::string16 search_terms;
    if (template_url->ExtractSearchTermsFromURL(
        stripped_destination_url,
        template_url_service->search_terms_data(),
        &search_terms)) {
      stripped_destination_url =
          GURL(template_url->url_ref().ReplaceSearchTerms(
              TemplateURLRef::SearchTermsArgs(search_terms),
              template_url_service->search_terms_data()));
    }
  }

  // |replacements| keeps all the substitions we're going to make to
  // from {destination_url} to {stripped_destination_url}.  |need_replacement|
  // is a helper variable that helps us keep track of whether we need
  // to apply the replacement.
  bool needs_replacement = false;
  GURL::Replacements replacements;

  // Remove the www. prefix from the host.
  static const char prefix[] = "www.";
  static const size_t prefix_len = arraysize(prefix) - 1;
  std::string host = stripped_destination_url.host();
  if (host.compare(0, prefix_len, prefix) == 0) {
    host = host.substr(prefix_len);
    replacements.SetHostStr(host);
    needs_replacement = true;
  }

  // Replace https protocol with http protocol.
  if (stripped_destination_url.SchemeIs(url::kHttpsScheme)) {
    replacements.SetScheme(url::kHttpScheme,
                           url::Component(0, strlen(url::kHttpScheme)));
    needs_replacement = true;
  }

  if (needs_replacement)
    stripped_destination_url = stripped_destination_url.ReplaceComponents(
        replacements);
  return stripped_destination_url;
}

void AutocompleteMatch::ComputeStrippedDestinationURL(
    TemplateURLService* template_url_service) {
  stripped_destination_url =
      GURLToStrippedGURL(destination_url, template_url_service, keyword);
}

void AutocompleteMatch::EnsureUWYTIsAllowedToBeDefault(
    const GURL& canonical_input_url,
    TemplateURLService* template_url_service) {
  if (!allowed_to_be_default_match) {
    const GURL& stripped_canonical_input_url =
        AutocompleteMatch::GURLToStrippedGURL(
            canonical_input_url, template_url_service, base::string16());
    ComputeStrippedDestinationURL(template_url_service);
    allowed_to_be_default_match =
        stripped_canonical_input_url == stripped_destination_url;
  }
}

void AutocompleteMatch::GetKeywordUIState(
    TemplateURLService* template_url_service,
    base::string16* keyword,
    bool* is_keyword_hint) const {
  *is_keyword_hint = associated_keyword.get() != NULL;
  keyword->assign(*is_keyword_hint ? associated_keyword->keyword :
      GetSubstitutingExplicitlyInvokedKeyword(template_url_service));
}

base::string16 AutocompleteMatch::GetSubstitutingExplicitlyInvokedKeyword(
    TemplateURLService* template_url_service) const {
  if (transition != content::PAGE_TRANSITION_KEYWORD ||
      template_url_service == NULL) {
    return base::string16();
  }

  const TemplateURL* t_url = GetTemplateURL(template_url_service, false);
  return (t_url &&
          t_url->SupportsReplacement(
              template_url_service->search_terms_data())) ?
      keyword : base::string16();
}

TemplateURL* AutocompleteMatch::GetTemplateURL(
    TemplateURLService* template_url_service,
    bool allow_fallback_to_destination_host) const {
  return GetTemplateURLWithKeyword(
      template_url_service, keyword,
      allow_fallback_to_destination_host ?
          destination_url.host() : std::string());
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const std::string& value) {
  DCHECK(!property.empty());
  DCHECK(!value.empty());
  additional_info[property] = value;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             int value) {
  RecordAdditionalInfo(property, base::IntToString(value));
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const base::Time& value) {
  RecordAdditionalInfo(property,
                       base::UTF16ToUTF8(
                           base::TimeFormatShortDateAndTime(value)));
}

std::string AutocompleteMatch::GetAdditionalInfo(
    const std::string& property) const {
  AdditionalInfo::const_iterator i(additional_info.find(property));
  return (i == additional_info.end()) ? std::string() : i->second;
}

bool AutocompleteMatch::IsVerbatimType() const {
  const bool is_keyword_verbatim_match =
      (type == AutocompleteMatchType::SEARCH_OTHER_ENGINE &&
       provider != NULL &&
       provider->type() == AutocompleteProvider::TYPE_SEARCH);
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
      type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
      is_keyword_verbatim_match;
}

bool AutocompleteMatch::SupportsDeletion() const {
  if (deletable)
    return true;

  for (ACMatches::const_iterator it(duplicate_matches.begin());
       it != duplicate_matches.end(); ++it) {
    if (it->deletable)
      return true;
  }
  return false;
}

#ifndef NDEBUG
void AutocompleteMatch::Validate() const {
  ValidateClassifications(contents, contents_class);
  ValidateClassifications(description, description_class);
}

void AutocompleteMatch::ValidateClassifications(
    const base::string16& text,
    const ACMatchClassifications& classifications) const {
  if (text.empty()) {
    DCHECK(classifications.empty());
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(!classifications.empty()) << "No classification for \"" << text << '"';
  DCHECK_EQ(0U, classifications[0].offset)
      << "Classification misses beginning for \"" << text << '"';
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (ACMatchClassifications::const_iterator i(classifications.begin() + 1);
       i != classifications.end(); ++i) {
    const char* provider_name = provider ? provider->GetName() : "None";
    DCHECK_GT(i->offset, last_offset)
        << " Classification for \"" << text << "\" with offset of " << i->offset
        << " is unsorted in relation to last offset of " << last_offset
        << ". Provider: " << provider_name << ".";
    DCHECK_LT(i->offset, text.length())
        << " Classification of [" << i->offset << "," << text.length()
        << "] is out of bounds for \"" << text << "\". Provider: "
        << provider_name << ".";
    last_offset = i->offset;
  }
}
#endif
