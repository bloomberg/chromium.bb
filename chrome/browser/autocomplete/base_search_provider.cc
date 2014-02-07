// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/base_search_provider.h"

#include "base/json/json_string_value_serializer.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

// BaseSearchProvider ---------------------------------------------------------

BaseSearchProvider::BaseSearchProvider(AutocompleteProviderListener* listener,
                                       Profile* profile,
                                       AutocompleteProvider::Type type)
    : AutocompleteProvider(listener, profile, type),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false) {}

void BaseSearchProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  std::vector<uint32> field_trial_hashes;
  OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
  for (size_t i = 0; i < field_trial_hashes.size(); ++i) {
    if (field_trial_triggered_)
      new_entry.mutable_field_trial_triggered()->Add(field_trial_hashes[i]);
    if (field_trial_triggered_in_session_) {
      new_entry.mutable_field_trial_triggered_in_session()->Add(
          field_trial_hashes[i]);
     }
  }
}

BaseSearchProvider::~BaseSearchProvider() {}

// BaseSearchProvider::Result --------------------------------------------------

BaseSearchProvider::Result::Result(bool from_keyword_provider,
                                   int relevance,
                                   bool relevance_from_server)
    : from_keyword_provider_(from_keyword_provider),
      relevance_(relevance),
      relevance_from_server_(relevance_from_server) {}

BaseSearchProvider::Result::~Result() {}

// BaseSearchProvider::SuggestResult -------------------------------------------

BaseSearchProvider::SuggestResult::SuggestResult(
    const base::string16& suggestion,
    AutocompleteMatchType::Type type,
    const base::string16& match_contents,
    const base::string16& annotation,
    const std::string& suggest_query_params,
    const std::string& deletion_url,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    bool should_prefetch,
    const base::string16& input_text)
    : Result(from_keyword_provider, relevance, relevance_from_server),
      suggestion_(suggestion),
      type_(type),
      annotation_(annotation),
      suggest_query_params_(suggest_query_params),
      deletion_url_(deletion_url),
      should_prefetch_(should_prefetch) {
  match_contents_ = match_contents;
  DCHECK(!match_contents_.empty());
  ClassifyMatchContents(true, input_text);
}

BaseSearchProvider::SuggestResult::~SuggestResult() {}

void BaseSearchProvider::SuggestResult::ClassifyMatchContents(
    const bool allow_bolding_all,
    const base::string16& input_text) {
  size_t input_position = match_contents_.find(input_text);
  if (!allow_bolding_all && (input_position == base::string16::npos)) {
    // Bail if the code below to update the bolding would bold the whole
    // string.  Note that the string may already be entirely bolded; if
    // so, leave it as is.
    return;
  }
  match_contents_class_.clear();
  // We do intra-string highlighting for suggestions - the suggested segment
  // will be highlighted, e.g. for input_text = "you" the suggestion may be
  // "youtube", so we'll bold the "tube" section: you*tube*.
  if (input_text != match_contents_) {
    if (input_position == base::string16::npos) {
      // The input text is not a substring of the query string, e.g. input
      // text is "slasdot" and the query string is "slashdot", so we bold the
      // whole thing.
      match_contents_class_.push_back(
          ACMatchClassification(0, ACMatchClassification::MATCH));
    } else {
      // We don't iterate over the string here annotating all matches because
      // it looks odd to have every occurrence of a substring that may be as
      // short as a single character highlighted in a query suggestion result,
      // e.g. for input text "s" and query string "southwest airlines", it
      // looks odd if both the first and last s are highlighted.
      if (input_position != 0) {
        match_contents_class_.push_back(
            ACMatchClassification(0, ACMatchClassification::MATCH));
      }
      match_contents_class_.push_back(
          ACMatchClassification(input_position, ACMatchClassification::NONE));
      size_t next_fragment_position = input_position + input_text.length();
      if (next_fragment_position < match_contents_.length()) {
        match_contents_class_.push_back(ACMatchClassification(
            next_fragment_position, ACMatchClassification::MATCH));
      }
    }
  } else {
    // Otherwise, match_contents_ is a verbatim (what-you-typed) match, either
    // for the default provider or a keyword search provider.
    match_contents_class_.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }
}

bool BaseSearchProvider::SuggestResult::IsInlineable(
    const base::string16& input) const {
  return StartsWith(suggestion_, input, false);
}

int BaseSearchProvider::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_provider_ && keyword_provider_requested)
    return 100;
  return ((input.type() == AutocompleteInput::URL) ? 300 : 600);
}

// BaseSearchProvider::NavigationResult ----------------------------------------

BaseSearchProvider::NavigationResult::NavigationResult(
    const AutocompleteProvider& provider,
    const GURL& url,
    const base::string16& description,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    const base::string16& input_text,
    const std::string& languages)
    : Result(from_keyword_provider, relevance, relevance_from_server),
      url_(url),
      formatted_url_(AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          provider.StringForURLDisplay(url, true, false))),
      description_(description) {
  DCHECK(url_.is_valid());
  CalculateAndClassifyMatchContents(true, input_text, languages);
}

BaseSearchProvider::NavigationResult::~NavigationResult() {}

void BaseSearchProvider::NavigationResult::CalculateAndClassifyMatchContents(
    const bool allow_bolding_nothing,
    const base::string16& input_text,
    const std::string& languages) {
  // First look for the user's input inside the formatted url as it would be
  // without trimming the scheme, so we can find matches at the beginning of the
  // scheme.
  const URLPrefix* prefix =
      URLPrefix::BestURLPrefix(formatted_url_, input_text);
  size_t match_start = (prefix == NULL) ? formatted_url_.find(input_text)
                                        : prefix->prefix.length();
  bool trim_http = !AutocompleteInput::HasHTTPScheme(input_text) &&
                   (!prefix || (match_start != 0));
  const net::FormatUrlTypes format_types =
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP);

  base::string16 match_contents = net::FormatUrl(url_, languages, format_types,
      net::UnescapeRule::SPACES, NULL, NULL, &match_start);
  // If the first match in the untrimmed string was inside a scheme that we
  // trimmed, look for a subsequent match.
  if (match_start == base::string16::npos)
    match_start = match_contents.find(input_text);
  // Update |match_contents_| and |match_contents_class_| if it's allowed.
  if (allow_bolding_nothing || (match_start != base::string16::npos)) {
    match_contents_ = match_contents;
    // Safe if |match_start| is npos; also safe if the input is longer than the
    // remaining contents after |match_start|.
    AutocompleteMatch::ClassifyLocationInString(match_start,
        input_text.length(), match_contents_.length(),
        ACMatchClassification::URL, &match_contents_class_);
  }
}

bool BaseSearchProvider::NavigationResult::IsInlineable(
    const base::string16& input) const {
  return URLPrefix::BestURLPrefix(formatted_url_, input) != NULL;
}

int BaseSearchProvider::NavigationResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  return (from_keyword_provider_ || !keyword_provider_requested) ? 800 : 150;
}

// BaseSearchProvider::Results -------------------------------------------------

BaseSearchProvider::Results::Results() : verbatim_relevance(-1) {}

BaseSearchProvider::Results::~Results() {}

void BaseSearchProvider::Results::Clear() {
  suggest_results.clear();
  navigation_results.clear();
  verbatim_relevance = -1;
  metadata.clear();
}

bool BaseSearchProvider::Results::HasServerProvidedScores() const {
  if (verbatim_relevance >= 0)
    return true;

  // Right now either all results of one type will be server-scored or they will
  // all be locally scored, but in case we change this later, we'll just check
  // them all.
  for (SuggestResults::const_iterator i(suggest_results.begin());
       i != suggest_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }
  for (NavigationResults::const_iterator i(navigation_results.begin());
       i != navigation_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }

  return false;
}

// BaseSearchProvider ---------------------------------------------------------

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const base::string16& input_text,
    const SuggestResult& suggestion,
    const TemplateURL* template_url,
    int accepted_suggestion,
    int omnibox_start_margin,
    bool append_extra_query_params) {
  AutocompleteMatch match(autocomplete_provider, suggestion.relevance(), false,
                          suggestion.type());

  if (!template_url)
    return match;
  match.keyword = template_url->keyword();
  match.contents = suggestion.match_contents();
  match.contents_class = suggestion.match_contents_class();

  if (!suggestion.annotation().empty())
    match.description = suggestion.annotation();

  match.allowed_to_be_default_match =
      (input_text == suggestion.match_contents());

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  if (input.type() == AutocompleteInput::FORCED_QUERY)
    match.fill_into_edit.assign(base::ASCIIToUTF16("?"));
  if (suggestion.from_keyword_provider())
    match.fill_into_edit.append(match.keyword + base::char16(' '));
  if (!input.prevent_inline_autocomplete() &&
      StartsWith(suggestion.suggestion(), input_text, false)) {
    match.inline_autocompletion =
        suggestion.suggestion().substr(input_text.length());
    match.allowed_to_be_default_match = true;
  }
  match.fill_into_edit.append(suggestion.suggestion());

  const TemplateURLRef& search_url = template_url->url_ref();
  DCHECK(search_url.SupportsReplacement());
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(suggestion.suggestion()));
  match.search_terms_args->original_query = input_text;
  match.search_terms_args->accepted_suggestion = accepted_suggestion;
  match.search_terms_args->omnibox_start_margin = omnibox_start_margin;
  match.search_terms_args->suggest_query_params =
      suggestion.suggest_query_params();
  match.search_terms_args->append_extra_query_params =
      append_extra_query_params;
  // This is the destination URL sans assisted query stats.  This must be set
  // so the AutocompleteController can properly de-dupe; the controller will
  // eventually overwrite it before it reaches the user.
  match.destination_url =
      GURL(search_url.ReplaceSearchTerms(*match.search_terms_args.get()));

  // Search results don't look like URLs.
  match.transition = suggestion.from_keyword_provider() ?
      content::PAGE_TRANSITION_KEYWORD : content::PAGE_TRANSITION_GENERATED;

  return match;
}

// static
scoped_ptr<base::Value> BaseSearchProvider::DeserializeJsonData(
    std::string json_data) {
  // The JSON response should be an array.
  for (size_t response_start_index = json_data.find("["), i = 0;
       response_start_index != std::string::npos && i < 5;
       response_start_index = json_data.find("[", 1), i++) {
    // Remove any XSSI guards to allow for JSON parsing.
    if (response_start_index > 0)
      json_data.erase(0, response_start_index);

    JSONStringValueSerializer deserializer(json_data);
    deserializer.set_allow_trailing_comma(true);
    int error_code = 0;
    scoped_ptr<base::Value> data(deserializer.Deserialize(&error_code, NULL));
    if (error_code == 0)
      return data.Pass();
  }
  return scoped_ptr<base::Value>();
}

// static
bool BaseSearchProvider::CanSendURL(
    const GURL& current_page_url,
    const GURL& suggest_url,
    const TemplateURL* template_url,
    AutocompleteInput::PageClassification page_classification,
    Profile* profile) {
  if (!current_page_url.is_valid())
    return false;

  // TODO(hfung): Show Most Visited on NTP with appropriate verbatim
  // description when the user actively focuses on the omnibox as discussed in
  // crbug/305366 if Most Visited (or something similar) will launch.
  if ((page_classification ==
       AutocompleteInput::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
      (page_classification ==
       AutocompleteInput::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS))
    return false;

  // Only allow HTTP URLs or HTTPS URLs for the same domain as the search
  // provider.
  if ((current_page_url.scheme() != content::kHttpScheme) &&
      ((current_page_url.scheme() != content::kHttpsScheme) ||
       !net::registry_controlled_domains::SameDomainOrHost(
           current_page_url, suggest_url,
           net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)))
    return false;

  // Make sure we are sending the suggest request through HTTPS to prevent
  // exposing the current page URL to networks before the search provider.
  if (!suggest_url.SchemeIs(content::kHttpsScheme))
    return false;

  // Don't run if there's no profile or in incognito mode.
  if (profile == NULL || profile->IsOffTheRecord())
    return false;

  // Don't run if we can't get preferences or search suggest is not enabled.
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->GetBoolean(prefs::kSearchSuggestEnabled))
    return false;

  // Only make the request if we know that the provider supports zero suggest
  // (currently only the prepopulated Google provider).
  if (template_url == NULL || !template_url->SupportsReplacement() ||
      TemplateURLPrepopulateData::GetEngineType(*template_url) !=
      SEARCH_ENGINE_GOOGLE)
    return false;

  // Check field trials and settings allow sending the URL on suggest requests.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  browser_sync::SyncPrefs sync_prefs(prefs);
  if (!OmniboxFieldTrial::InZeroSuggestFieldTrial() ||
      service == NULL ||
      !service->IsSyncEnabledAndLoggedIn() ||
      !sync_prefs.GetPreferredDataTypes(syncer::UserTypes()).Has(
          syncer::PROXY_TABS) ||
      service->GetEncryptedDataTypes().Has(syncer::SESSIONS))
    return false;

  return true;
}

