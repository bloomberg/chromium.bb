// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/base_search_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

// SuggestionDeletionHandler -------------------------------------------------

// This class handles making requests to the server in order to delete
// personalized suggestions.
class SuggestionDeletionHandler : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(bool, SuggestionDeletionHandler*)>
      DeletionCompletedCallback;

  SuggestionDeletionHandler(
      const std::string& deletion_url,
      net::URLRequestContextGetter* request_context,
      const DeletionCompletedCallback& callback);

  ~SuggestionDeletionHandler() override;

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  std::unique_ptr<net::URLFetcher> deletion_fetcher_;
  DeletionCompletedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionDeletionHandler);
};

SuggestionDeletionHandler::SuggestionDeletionHandler(
    const std::string& deletion_url,
    net::URLRequestContextGetter* request_context,
    const DeletionCompletedCallback& callback) : callback_(callback) {
  GURL url(deletion_url);
  DCHECK(url.is_valid());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("omnibox_suggest_deletion", R"(
        semantics {
          sender: "Omnibox"
          description:
            "When users attempt to delete server-provided personalized search "
            "or navigation suggestions from the omnibox dropdown, Chrome sends "
            "a message to the server requesting deletion of the suggestion."
          trigger:
            "A user attempt to delete a server-provided omnibox suggestion, "
            "for which the server provided a custom deletion URL."
          data:
            "No user data is explicitly sent with the request, but because the "
            "requested URL is provided by the server for each specific "
            "suggestion, it necessarily uniquely identifies the suggestion the "
            "user is attempting to delete."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Since this can only be triggered on seeing server-provided "
            "suggestions in the omnibox dropdown, whether it is enabled is the "
            "same as whether those suggestions are enabled.\n"
            "Users can control this feature via the 'Use a prediction service "
            "to help complete searches and URLs typed in the address bar' "
            "setting under 'Privacy'. The feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");
  deletion_fetcher_ =
      net::URLFetcher::Create(BaseSearchProvider::kDeletionURLFetcherID, url,
                              net::URLFetcher::GET, this, traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      deletion_fetcher_.get(), data_use_measurement::DataUseUserData::OMNIBOX);
  deletion_fetcher_->SetRequestContext(request_context);
  deletion_fetcher_->Start();
}

SuggestionDeletionHandler::~SuggestionDeletionHandler() {
}

void SuggestionDeletionHandler::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == deletion_fetcher_.get());
  callback_.Run(
      source->GetStatus().is_success() && (source->GetResponseCode() == 200),
      this);
}

// BaseSearchProvider ---------------------------------------------------------

// static
const int BaseSearchProvider::kDefaultProviderURLFetcherID = 1;
const int BaseSearchProvider::kKeywordProviderURLFetcherID = 2;
const int BaseSearchProvider::kDeletionURLFetcherID = 3;

BaseSearchProvider::BaseSearchProvider(AutocompleteProvider::Type type,
                                       AutocompleteProviderClient* client)
    : AutocompleteProvider(type),
      client_(client),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false) {
}

// static
bool BaseSearchProvider::ShouldPrefetch(const AutocompleteMatch& match) {
  return match.GetAdditionalInfo(kShouldPrefetchKey) == kTrue;
}

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    const base::string16& suggestion,
    AutocompleteMatchType::Type type,
    bool from_keyword_provider,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data) {
  // These calls use a number of default values.  For instance, they assume
  // that if this match is from a keyword provider, then the user is in keyword
  // mode.  They also assume the caller knows what it's doing and we set
  // this match to look as if it was received/created synchronously.
  SearchSuggestionParser::SuggestResult suggest_result(
      suggestion, type,
      /*subtype_identifier=*/0,
      /*match_contents=*/suggestion,
      /*match_contents_prefix=*/base::string16(),
      /*annotation=*/base::string16(),
      /*answer_contents=*/base::string16(),
      /*answer_type=*/base::string16(),
      /*answer=*/nullptr,
      /*suggest_query_params=*/std::string(),
      /*deletion_url=*/std::string(), from_keyword_provider,
      /*relevance=*/0,
      /*relevance_from_server=*/false,
      /*should_prefetch=*/false,
      /*input_text=*/base::string16());
  suggest_result.set_received_after_last_keystroke(false);
  return CreateSearchSuggestion(nullptr, AutocompleteInput(),
                                from_keyword_provider, suggest_result,
                                template_url, search_terms_data, 0, false);
}

void BaseSearchProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  if (!match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey).empty()) {
    deletion_handlers_.push_back(std::make_unique<SuggestionDeletionHandler>(
        match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey),
        client_->GetRequestContext(),
        base::Bind(&BaseSearchProvider::OnDeletionComplete,
                   base::Unretained(this))));
  }

  const TemplateURL* template_url =
      match.GetTemplateURL(client_->GetTemplateURLService(), false);
  // This may be nullptr if the template corresponding to the keyword has been
  // deleted or there is no keyword set.
  if (template_url != nullptr) {
    client_->DeleteMatchingURLsForKeywordFromHistory(template_url->id(),
                                                     match.contents);
  }

  // Immediately update the list of matches to show the match was deleted,
  // regardless of whether the server request actually succeeds.
  DeleteMatchFromMatches(match);
}

void BaseSearchProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  std::vector<uint32_t> field_trial_hashes;
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

// static
const char BaseSearchProvider::kRelevanceFromServerKey[] =
    "relevance_from_server";
const char BaseSearchProvider::kShouldPrefetchKey[] = "should_prefetch";
const char BaseSearchProvider::kSuggestMetadataKey[] = "suggest_metadata";
const char BaseSearchProvider::kDeletionUrlKey[] = "deletion_url";
const char BaseSearchProvider::kTrue[] = "true";
const char BaseSearchProvider::kFalse[] = "false";

BaseSearchProvider::~BaseSearchProvider() {}

// static
bool BaseSearchProvider::IsNTPPage(
    metrics::OmniboxEventProto::PageClassification classification) {
  using OEP = metrics::OmniboxEventProto;
  return (classification == OEP::NTP) ||
         (classification == OEP::OBSOLETE_INSTANT_NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
}

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const bool in_keyword_mode,
    const SearchSuggestionParser::SuggestResult& suggestion,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion,
    bool append_extra_query_params) {
  AutocompleteMatch match(autocomplete_provider, suggestion.relevance(), false,
                          suggestion.type());

  if (!template_url)
    return match;
  match.keyword = template_url->keyword();
  match.contents = suggestion.match_contents();
  match.contents_class = suggestion.match_contents_class();
  match.answer_contents = suggestion.answer_contents();
  match.answer_type = suggestion.answer_type();
  match.answer = SuggestionAnswer::copy(suggestion.answer());
  match.subtype_identifier = suggestion.subtype_identifier();
  if (suggestion.type() == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    match.RecordAdditionalInfo(kACMatchPropertySuggestionText,
                               base::UTF16ToUTF8(suggestion.suggestion()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsPrefix,
        base::UTF16ToUTF8(suggestion.match_contents_prefix()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsStartIndex,
        static_cast<int>(
            suggestion.suggestion().length() - match.contents.length()));
  }

  if (!suggestion.annotation().empty()) {
    match.description = suggestion.annotation();
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &match.description_class, 0, ACMatchClassification::NONE);
  }

  const base::string16 input_lower = base::i18n::ToLower(input.text());
  // suggestion.match_contents() should have already been collapsed.
  match.allowed_to_be_default_match =
      (!in_keyword_mode || suggestion.from_keyword_provider()) &&
      (base::CollapseWhitespace(input_lower, false) ==
       base::i18n::ToLower(suggestion.match_contents()));

  if (suggestion.from_keyword_provider())
    match.fill_into_edit.append(match.keyword + base::char16(' '));
  // We only allow inlinable navsuggestions that were received before the
  // last keystroke because we don't want asynchronous inline autocompletions.
  if (!input.prevent_inline_autocomplete() &&
      !suggestion.received_after_last_keystroke() &&
      (!in_keyword_mode || suggestion.from_keyword_provider()) &&
      base::StartsWith(
          base::i18n::ToLower(suggestion.suggestion()), input_lower,
          base::CompareCase::SENSITIVE)) {
    match.inline_autocompletion =
        suggestion.suggestion().substr(input.text().length());
    match.allowed_to_be_default_match = true;
  }
  match.fill_into_edit.append(suggestion.suggestion());

  const TemplateURLRef& search_url = template_url->url_ref();
  DCHECK(search_url.SupportsReplacement(search_terms_data));
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(suggestion.suggestion()));
  match.search_terms_args->original_query = input.text();
  match.search_terms_args->accepted_suggestion = accepted_suggestion;
  match.search_terms_args->suggest_query_params =
      suggestion.suggest_query_params();
  match.search_terms_args->append_extra_query_params =
      append_extra_query_params;
  // This is the destination URL sans assisted query stats.  This must be set
  // so the AutocompleteController can properly de-dupe; the controller will
  // eventually overwrite it before it reaches the user.
  match.destination_url =
      GURL(search_url.ReplaceSearchTerms(*match.search_terms_args.get(),
                                         search_terms_data));

  // Search results don't look like URLs.
  match.transition = suggestion.from_keyword_provider() ?
      ui::PAGE_TRANSITION_KEYWORD : ui::PAGE_TRANSITION_GENERATED;

  return match;
}

// static
bool BaseSearchProvider::CanSendURL(
    const GURL& current_page_url,
    const GURL& suggest_url,
    const TemplateURL* template_url,
    metrics::OmniboxEventProto::PageClassification page_classification,
    const SearchTermsData& search_terms_data,
    AutocompleteProviderClient* client) {
  // Make sure we are sending the suggest request through a cryptographically
  // secure channel to prevent exposing the current page URL or personalized
  // results without encryption.
  if (!suggest_url.SchemeIsCryptographic())
    return false;

  // Don't run if in incognito mode.
  if (client->IsOffTheRecord())
    return false;

  // Don't run if we can't get preferences or search suggest is not enabled.
  if (!client->SearchSuggestEnabled())
    return false;

  // Only make the request if we know that the provider supports sending zero
  // suggest. (currently only the prepopulated Google provider).
  if (template_url == nullptr ||
      !template_url->SupportsReplacement(search_terms_data) ||
      template_url->GetEngineType(search_terms_data) != SEARCH_ENGINE_GOOGLE)
    return false;

  if (!current_page_url.is_valid())
    return false;

  // Don't bother sending the URL of an NTP page; it's not useful.  The server
  // already gets equivalent information in the form of the current page
  // classification.
  if (IsNTPPage(page_classification))
    return false;

  // Only allow HTTP URLs or HTTPS URLs.
  const bool scheme_allowed = (current_page_url.scheme() == url::kHttpScheme) ||
                              (current_page_url.scheme() == url::kHttpsScheme);
  if (!scheme_allowed)
    return false;

  if (!client->IsTabUploadToGoogleActive())
    return false;

  return true;
}

void BaseSearchProvider::SetDeletionURL(const std::string& deletion_url,
                                        AutocompleteMatch* match) {
  if (deletion_url.empty())
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service)
    return;
  GURL url =
      template_url_service->GetDefaultSearchProvider()->GenerateSearchURL(
          template_url_service->search_terms_data());
  url = url.GetOrigin().Resolve(deletion_url);
  if (url.is_valid()) {
    match->RecordAdditionalInfo(BaseSearchProvider::kDeletionUrlKey,
                                url.spec());
    match->deletable = true;
  }
}

void BaseSearchProvider::AddMatchToMap(
    const SearchSuggestionParser::SuggestResult& result,
    const std::string& metadata,
    int accepted_suggestion,
    bool mark_as_deletable,
    bool in_keyword_mode,
    MatchMap* map) {
  AutocompleteMatch match = CreateSearchSuggestion(
      this, GetInput(result.from_keyword_provider()), in_keyword_mode, result,
      GetTemplateURL(result.from_keyword_provider()),
      client_->GetTemplateURLService()->search_terms_data(),
      accepted_suggestion, ShouldAppendExtraParams(result));
  if (!match.destination_url.is_valid())
    return;
  match.RecordAdditionalInfo(kRelevanceFromServerKey,
                             result.relevance_from_server() ? kTrue : kFalse);
  match.RecordAdditionalInfo(kShouldPrefetchKey,
                             result.should_prefetch() ? kTrue : kFalse);
  SetDeletionURL(result.deletion_url(), &match);
  if (mark_as_deletable)
    match.deletable = true;
  // Metadata is needed only for prefetching queries.
  if (result.should_prefetch())
    match.RecordAdditionalInfo(kSuggestMetadataKey, metadata);

  // Try to add |match| to |map|.  If a match for this suggestion is
  // already in |map|, replace it if |match| is more relevant.
  // NOTE: Keep this ToLower() call in sync with url_database.cc.
  MatchKey match_key(
      std::make_pair(base::i18n::ToLower(result.suggestion()),
                     match.search_terms_args->suggest_query_params));
  const std::pair<MatchMap::iterator, bool> i(
       map->insert(std::make_pair(match_key, match)));

  bool should_prefetch = result.should_prefetch();
  if (!i.second) {
    // NOTE: We purposefully do a direct relevance comparison here instead of
    // using AutocompleteMatch::MoreRelevant(), so that we'll prefer "items
    // added first" rather than "items alphabetically first" when the scores
    // are equal. The only case this matters is when a user has results with
    // the same score that differ only by capitalization; because the history
    // system returns results sorted by recency, this means we'll pick the most
    // recent such result even if the precision of our relevance score is too
    // low to distinguish the two.
    if (match.relevance > i.first->second.relevance) {
      match.duplicate_matches.insert(match.duplicate_matches.end(),
                                     i.first->second.duplicate_matches.begin(),
                                     i.first->second.duplicate_matches.end());
      i.first->second.duplicate_matches.clear();
      match.duplicate_matches.push_back(i.first->second);
      i.first->second = match;
    } else {
      i.first->second.duplicate_matches.push_back(match);
      if (match.keyword == i.first->second.keyword) {
        // Old and new matches are from the same search provider. It is okay to
        // record one match's prefetch data onto a different match (for the same
        // query string) for the following reasons:
        // 1. Because the suggest server only sends down a query string from
        // which we construct a URL, rather than sending a full URL, and because
        // we construct URLs from query strings in the same way every time, the
        // URLs for the two matches will be the same. Therefore, we won't end up
        // prefetching something the server didn't intend.
        // 2. Presumably the server sets the prefetch bit on a match it things
        // is sufficiently relevant that the user is likely to choose it.
        // Surely setting the prefetch bit on a match of even higher relevance
        // won't violate this assumption.
        should_prefetch |= ShouldPrefetch(i.first->second);
        i.first->second.RecordAdditionalInfo(kShouldPrefetchKey,
                                             should_prefetch ? kTrue : kFalse);
        if (should_prefetch)
          i.first->second.RecordAdditionalInfo(kSuggestMetadataKey, metadata);
      }
    }
    // Copy over answer data from lower-ranking item, if necessary.
    // This depends on the lower-ranking item always being added last - see
    // use of push_back above.
    AutocompleteMatch& more_relevant_match = i.first->second;
    const AutocompleteMatch& less_relevant_match =
        more_relevant_match.duplicate_matches.back();
    if (less_relevant_match.answer && !more_relevant_match.answer) {
      more_relevant_match.answer_type = less_relevant_match.answer_type;
      more_relevant_match.answer_contents = less_relevant_match.answer_contents;
      more_relevant_match.answer =
          SuggestionAnswer::copy(less_relevant_match.answer.get());
    }
  }
}

bool BaseSearchProvider::ParseSuggestResults(
    const base::Value& root_val,
    int default_result_relevance,
    bool is_keyword_result,
    SearchSuggestionParser::Results* results) {
  if (!SearchSuggestionParser::ParseSuggestResults(
          root_val, GetInput(is_keyword_result), client_->GetSchemeClassifier(),
          default_result_relevance, is_keyword_result, results))
    return false;

  for (const GURL& url : results->answers_image_urls)
    client_->PrefetchImage(url);

  field_trial_triggered_ |= results->field_trial_triggered;
  field_trial_triggered_in_session_ |= results->field_trial_triggered;
  return true;
}

void BaseSearchProvider::DeleteMatchFromMatches(
    const AutocompleteMatch& match) {
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i) {
    // Find the desired match to delete by checking the type and contents.
    // We can't check the destination URL, because the autocomplete controller
    // may have reformulated that. Not that while checking for matching
    // contents works for personalized suggestions, if more match types gain
    // deletion support, this algorithm may need to be re-examined.
    if (i->contents == match.contents && i->type == match.type) {
      matches_.erase(i);
      break;
    }
  }
}

void BaseSearchProvider::OnDeletionComplete(
    bool success, SuggestionDeletionHandler* handler) {
  RecordDeletionResult(success);
  deletion_handlers_.erase(std::remove_if(
      deletion_handlers_.begin(), deletion_handlers_.end(),
      [handler](const std::unique_ptr<SuggestionDeletionHandler>& elem) {
        return elem.get() == handler;
      }));
}
