// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/base_search_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/autocomplete/url_prefix.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/url_fixer/url_fixer.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

using metrics::OmniboxEventProto;

namespace {

AutocompleteMatchType::Type GetAutocompleteMatchType(const std::string& type) {
  if (type == "ENTITY")
    return AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
  if (type == "INFINITE")
    return AutocompleteMatchType::SEARCH_SUGGEST_INFINITE;
  if (type == "PERSONALIZED_QUERY")
    return AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED;
  if (type == "PROFILE")
    return AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
  if (type == "NAVIGATION")
    return AutocompleteMatchType::NAVSUGGEST;
  if (type == "PERSONALIZED_NAVIGATION")
    return AutocompleteMatchType::NAVSUGGEST_PERSONALIZED;
  return AutocompleteMatchType::SEARCH_SUGGEST;
}

} // namespace

// SuggestionDeletionHandler -------------------------------------------------

// This class handles making requests to the server in order to delete
// personalized suggestions.
class SuggestionDeletionHandler : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(bool, SuggestionDeletionHandler*)>
      DeletionCompletedCallback;

  SuggestionDeletionHandler(
      const std::string& deletion_url,
      Profile* profile,
      const DeletionCompletedCallback& callback);

  virtual ~SuggestionDeletionHandler();

 private:
  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  scoped_ptr<net::URLFetcher> deletion_fetcher_;
  DeletionCompletedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionDeletionHandler);
};

SuggestionDeletionHandler::SuggestionDeletionHandler(
    const std::string& deletion_url,
    Profile* profile,
    const DeletionCompletedCallback& callback) : callback_(callback) {
  GURL url(deletion_url);
  DCHECK(url.is_valid());

  deletion_fetcher_.reset(net::URLFetcher::Create(
      BaseSearchProvider::kDeletionURLFetcherID,
      url,
      net::URLFetcher::GET,
      this));
  deletion_fetcher_->SetRequestContext(profile->GetRequestContext());
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

BaseSearchProvider::BaseSearchProvider(AutocompleteProviderListener* listener,
                                       Profile* profile,
                                       AutocompleteProvider::Type type)
    : AutocompleteProvider(type),
      listener_(listener),
      profile_(profile),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false),
      suggest_results_pending_(0),
      in_app_list_(false) {
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
  return CreateSearchSuggestion(
      NULL, AutocompleteInput(), BaseSearchProvider::SuggestResult(
          suggestion, type, suggestion, base::string16(), base::string16(),
          base::string16(), base::string16(), std::string(), std::string(),
          from_keyword_provider, 0, false, false, base::string16()),
      template_url, search_terms_data, 0, 0, false, false);
}

void BaseSearchProvider::Stop(bool clear_cached_results) {
  StopSuggest();
  done_ = true;

  if (clear_cached_results)
    ClearAllResults();
}

void BaseSearchProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  if (!match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey).empty()) {
    deletion_handlers_.push_back(new SuggestionDeletionHandler(
        match.GetAdditionalInfo(BaseSearchProvider::kDeletionUrlKey),
        profile_,
        base::Bind(&BaseSearchProvider::OnDeletionComplete,
                   base::Unretained(this))));
  }

  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  TemplateURL* template_url = match.GetTemplateURL(template_url_service, false);
  // This may be NULL if the template corresponding to the keyword has been
  // deleted or there is no keyword set.
  if (template_url != NULL) {
    history_service->DeleteMatchingURLsForKeyword(template_url->id(),
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
  ModifyProviderInfo(&new_entry);
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

// BaseSearchProvider::Result --------------------------------------------------

BaseSearchProvider::Result::Result(bool from_keyword_provider,
                                   int relevance,
                                   bool relevance_from_server,
                                   AutocompleteMatchType::Type type,
                                   const std::string& deletion_url)
    : from_keyword_provider_(from_keyword_provider),
       type_(type),
       relevance_(relevance),
       relevance_from_server_(relevance_from_server),
       deletion_url_(deletion_url) {}

BaseSearchProvider::Result::~Result() {}

// BaseSearchProvider::SuggestResult -------------------------------------------

BaseSearchProvider::SuggestResult::SuggestResult(
    const base::string16& suggestion,
    AutocompleteMatchType::Type type,
    const base::string16& match_contents,
    const base::string16& match_contents_prefix,
    const base::string16& annotation,
    const base::string16& answer_contents,
    const base::string16& answer_type,
    const std::string& suggest_query_params,
    const std::string& deletion_url,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    bool should_prefetch,
    const base::string16& input_text)
    : Result(from_keyword_provider,
             relevance,
             relevance_from_server,
             type,
             deletion_url),
      suggestion_(suggestion),
      match_contents_prefix_(match_contents_prefix),
      annotation_(annotation),
      suggest_query_params_(suggest_query_params),
      answer_contents_(answer_contents),
      answer_type_(answer_type),
      should_prefetch_(should_prefetch) {
  match_contents_ = match_contents;
  DCHECK(!match_contents_.empty());
  ClassifyMatchContents(true, input_text);
}

BaseSearchProvider::SuggestResult::~SuggestResult() {}

void BaseSearchProvider::SuggestResult::ClassifyMatchContents(
    const bool allow_bolding_all,
    const base::string16& input_text) {
  if (input_text.empty()) {
    // In case of zero-suggest results, do not highlight matches.
    match_contents_class_.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    return;
  }

  base::string16 lookup_text = input_text;
  if (type_ == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE) {
    const size_t contents_index =
        suggestion_.length() - match_contents_.length();
    // Ensure the query starts with the input text, and ends with the match
    // contents, and the input text has an overlap with contents.
    if (StartsWith(suggestion_, input_text, true) &&
        EndsWith(suggestion_, match_contents_, true) &&
        (input_text.length() > contents_index)) {
      lookup_text = input_text.substr(contents_index);
    }
  }
  size_t lookup_position = match_contents_.find(lookup_text);
  if (!allow_bolding_all && (lookup_position == base::string16::npos)) {
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
    if (lookup_position == base::string16::npos) {
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
      if (lookup_position != 0) {
        match_contents_class_.push_back(
            ACMatchClassification(0, ACMatchClassification::MATCH));
      }
      match_contents_class_.push_back(
          ACMatchClassification(lookup_position, ACMatchClassification::NONE));
      size_t next_fragment_position = lookup_position + lookup_text.length();
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

int BaseSearchProvider::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_provider_ && keyword_provider_requested)
    return 100;
  return ((input.type() == metrics::OmniboxInputType::URL) ? 300 : 600);
}

// BaseSearchProvider::NavigationResult ----------------------------------------

BaseSearchProvider::NavigationResult::NavigationResult(
    const AutocompleteSchemeClassifier& scheme_classifier,
    const GURL& url,
    AutocompleteMatchType::Type type,
    const base::string16& description,
    const std::string& deletion_url,
    bool from_keyword_provider,
    int relevance,
    bool relevance_from_server,
    const base::string16& input_text,
    const std::string& languages)
    : Result(from_keyword_provider, relevance, relevance_from_server, type,
             deletion_url),
      url_(url),
      formatted_url_(AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, net::FormatUrl(url, languages,
                              net::kFormatUrlOmitAll & ~net::kFormatUrlOmitHTTP,
                              net::UnescapeRule::SPACES, NULL, NULL, NULL),
          scheme_classifier)),
      description_(description) {
  DCHECK(url_.is_valid());
  CalculateAndClassifyMatchContents(true, input_text, languages);
}

BaseSearchProvider::NavigationResult::~NavigationResult() {}

void BaseSearchProvider::NavigationResult::CalculateAndClassifyMatchContents(
    const bool allow_bolding_nothing,
    const base::string16& input_text,
    const std::string& languages) {
  if (input_text.empty()) {
    // In case of zero-suggest results, do not highlight matches.
    match_contents_class_.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    return;
  }

  // First look for the user's input inside the formatted url as it would be
  // without trimming the scheme, so we can find matches at the beginning of the
  // scheme.
  const URLPrefix* prefix =
      URLPrefix::BestURLPrefix(formatted_url_, input_text);
  size_t match_start = (prefix == NULL) ?
      formatted_url_.find(input_text) : prefix->prefix.length();
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

void BaseSearchProvider::SetDeletionURL(const std::string& deletion_url,
                                        AutocompleteMatch* match) {
  if (deletion_url.empty())
    return;
  TemplateURLService* template_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_service)
    return;
  GURL url = template_service->GetDefaultSearchProvider()->GenerateSearchURL(
      template_service->search_terms_data());
  url = url.GetOrigin().Resolve(deletion_url);
  if (url.is_valid()) {
    match->RecordAdditionalInfo(BaseSearchProvider::kDeletionUrlKey,
        url.spec());
    match->deletable = true;
  }
}

// BaseSearchProvider ---------------------------------------------------------

// static
AutocompleteMatch BaseSearchProvider::CreateSearchSuggestion(
    AutocompleteProvider* autocomplete_provider,
    const AutocompleteInput& input,
    const SuggestResult& suggestion,
    const TemplateURL* template_url,
    const SearchTermsData& search_terms_data,
    int accepted_suggestion,
    int omnibox_start_margin,
    bool append_extra_query_params,
    bool from_app_list) {
  AutocompleteMatch match(autocomplete_provider, suggestion.relevance(), false,
                          suggestion.type());

  if (!template_url)
    return match;
  match.keyword = template_url->keyword();
  match.contents = suggestion.match_contents();
  match.contents_class = suggestion.match_contents_class();
  match.answer_contents = suggestion.answer_contents();
  match.answer_type = suggestion.answer_type();
  if (suggestion.type() == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE) {
    match.RecordAdditionalInfo(
        kACMatchPropertyInputText, base::UTF16ToUTF8(input.text()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsPrefix,
        base::UTF16ToUTF8(suggestion.match_contents_prefix()));
    match.RecordAdditionalInfo(
        kACMatchPropertyContentsStartIndex,
        static_cast<int>(
            suggestion.suggestion().length() - match.contents.length()));
  }

  if (!suggestion.annotation().empty())
    match.description = suggestion.annotation();

  // suggestion.match_contents() should have already been collapsed.
  match.allowed_to_be_default_match =
      (base::CollapseWhitespace(input.text(), false) ==
       suggestion.match_contents());

  // When the user forced a query, we need to make sure all the fill_into_edit
  // values preserve that property.  Otherwise, if the user starts editing a
  // suggestion, non-Search results will suddenly appear.
  if (input.type() == metrics::OmniboxInputType::FORCED_QUERY)
    match.fill_into_edit.assign(base::ASCIIToUTF16("?"));
  if (suggestion.from_keyword_provider())
    match.fill_into_edit.append(match.keyword + base::char16(' '));
  if (!input.prevent_inline_autocomplete() &&
      StartsWith(suggestion.suggestion(), input.text(), false)) {
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
  match.search_terms_args->omnibox_start_margin = omnibox_start_margin;
  match.search_terms_args->suggest_query_params =
      suggestion.suggest_query_params();
  match.search_terms_args->append_extra_query_params =
      append_extra_query_params;
  match.search_terms_args->from_app_list = from_app_list;
  // This is the destination URL sans assisted query stats.  This must be set
  // so the AutocompleteController can properly de-dupe; the controller will
  // eventually overwrite it before it reaches the user.
  match.destination_url =
      GURL(search_url.ReplaceSearchTerms(*match.search_terms_args.get(),
                                         search_terms_data));

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
bool BaseSearchProvider::ZeroSuggestEnabled(
    const GURL& suggest_url,
    const TemplateURL* template_url,
    OmniboxEventProto::PageClassification page_classification,
    Profile* profile) {
  if (!OmniboxFieldTrial::InZeroSuggestFieldTrial())
    return false;

  // Make sure we are sending the suggest request through HTTPS to prevent
  // exposing the current page URL or personalized results without encryption.
  if (!suggest_url.SchemeIs(url::kHttpsScheme))
    return false;

  // Don't show zero suggest on the NTP.
  // TODO(hfung): Experiment with showing MostVisited zero suggest on NTP
  // under the conditions described in crbug.com/305366.
  if ((page_classification ==
       OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
      (page_classification ==
       OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS))
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
  UIThreadSearchTermsData search_terms_data(profile);
  if (template_url == NULL ||
      !template_url->SupportsReplacement(search_terms_data) ||
      TemplateURLPrepopulateData::GetEngineType(
          *template_url, search_terms_data) != SEARCH_ENGINE_GOOGLE)
    return false;

  return true;
}

// static
bool BaseSearchProvider::CanSendURL(
    const GURL& current_page_url,
    const GURL& suggest_url,
    const TemplateURL* template_url,
    OmniboxEventProto::PageClassification page_classification,
    Profile* profile) {
  if (!ZeroSuggestEnabled(suggest_url, template_url, page_classification,
                          profile))
    return false;

  if (!current_page_url.is_valid())
    return false;

  // Only allow HTTP URLs or HTTPS URLs for the same domain as the search
  // provider.
  if ((current_page_url.scheme() != url::kHttpScheme) &&
      ((current_page_url.scheme() != url::kHttpsScheme) ||
       !net::registry_controlled_domains::SameDomainOrHost(
           current_page_url, suggest_url,
           net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)))
    return false;

  // Check field trials and settings allow sending the URL on suggest requests.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  sync_driver::SyncPrefs sync_prefs(profile->GetPrefs());
  if (service == NULL ||
      !service->IsSyncEnabledAndLoggedIn() ||
      !sync_prefs.GetPreferredDataTypes(syncer::UserTypes()).Has(
          syncer::PROXY_TABS) ||
      service->GetEncryptedDataTypes().Has(syncer::SESSIONS))
    return false;

  return true;
}

void BaseSearchProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!done_);
  suggest_results_pending_--;
  DCHECK_GE(suggest_results_pending_, 0);  // Should never go negative.

  const bool is_keyword = IsKeywordFetcher(source);

  // Ensure the request succeeded and that the provider used is still available.
  // A verbatim match cannot be generated without this provider, causing errors.
  const bool request_succeeded =
      source->GetStatus().is_success() && (source->GetResponseCode() == 200) &&
      GetTemplateURL(is_keyword);

  LogFetchComplete(request_succeeded, is_keyword);

  bool results_updated = false;
  if (request_succeeded) {
    const net::HttpResponseHeaders* const response_headers =
        source->GetResponseHeaders();
    std::string json_data;
    source->GetResponseAsString(&json_data);

    // JSON is supposed to be UTF-8, but some suggest service providers send
    // JSON files in non-UTF-8 encodings.  The actual encoding is usually
    // specified in the Content-Type header field.
    if (response_headers) {
      std::string charset;
      if (response_headers->GetCharset(&charset)) {
        base::string16 data_16;
        // TODO(jungshik): Switch to CodePageToUTF8 after it's added.
        if (base::CodepageToUTF16(json_data, charset.c_str(),
                                  base::OnStringConversionError::FAIL,
                                  &data_16))
          json_data = base::UTF16ToUTF8(data_16);
      }
    }

    scoped_ptr<base::Value> data(DeserializeJsonData(json_data));
    if (data && StoreSuggestionResponse(json_data, *data.get()))
      return;

    results_updated = data.get() && ParseSuggestResults(
        *data.get(), is_keyword, GetResultsToFill(is_keyword));
  }

  UpdateMatches();
  if (done_ || results_updated)
    listener_->OnProviderUpdate(results_updated);
}

void BaseSearchProvider::AddMatchToMap(const SuggestResult& result,
                                       const std::string& metadata,
                                       int accepted_suggestion,
                                       bool mark_as_deletable,
                                       MatchMap* map) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile_);
  // Android and iOS have no InstantService.
  const int omnibox_start_margin = instant_service ?
      instant_service->omnibox_start_margin() : chrome::kDisableStartMargin;

  AutocompleteMatch match = CreateSearchSuggestion(
      this, GetInput(result.from_keyword_provider()), result,
      GetTemplateURL(result.from_keyword_provider()),
      UIThreadSearchTermsData(profile_), accepted_suggestion,
      omnibox_start_margin, ShouldAppendExtraParams(result),
      in_app_list_);
  if (!match.destination_url.is_valid())
    return;
  match.search_terms_args->bookmark_bar_pinned =
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
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
  }
}

bool BaseSearchProvider::ParseSuggestResults(const base::Value& root_val,
                                             bool is_keyword_result,
                                             Results* results) {
  base::string16 query;
  const base::ListValue* root_list = NULL;
  const base::ListValue* results_list = NULL;
  const AutocompleteInput& input = GetInput(is_keyword_result);

  if (!root_val.GetAsList(&root_list) || !root_list->GetString(0, &query) ||
      query != input.text() || !root_list->GetList(1, &results_list))
    return false;

  // 3rd element: Description list.
  const base::ListValue* descriptions = NULL;
  root_list->GetList(2, &descriptions);

  // 4th element: Disregard the query URL list for now.

  // Reset suggested relevance information.
  results->verbatim_relevance = -1;

  // 5th element: Optional key-value pairs from the Suggest server.
  const base::ListValue* types = NULL;
  const base::ListValue* relevances = NULL;
  const base::ListValue* suggestion_details = NULL;
  const base::DictionaryValue* extras = NULL;
  int prefetch_index = -1;
  if (root_list->GetDictionary(4, &extras)) {
    extras->GetList("google:suggesttype", &types);

    // Discard this list if its size does not match that of the suggestions.
    if (extras->GetList("google:suggestrelevance", &relevances) &&
        (relevances->GetSize() != results_list->GetSize()))
      relevances = NULL;
    extras->GetInteger("google:verbatimrelevance",
                       &results->verbatim_relevance);

    // Check if the active suggest field trial (if any) has triggered either
    // for the default provider or keyword provider.
    bool triggered = false;
    extras->GetBoolean("google:fieldtrialtriggered", &triggered);
    field_trial_triggered_ |= triggered;
    field_trial_triggered_in_session_ |= triggered;

    const base::DictionaryValue* client_data = NULL;
    if (extras->GetDictionary("google:clientdata", &client_data) && client_data)
      client_data->GetInteger("phi", &prefetch_index);

    if (extras->GetList("google:suggestdetail", &suggestion_details) &&
        suggestion_details->GetSize() != results_list->GetSize())
      suggestion_details = NULL;

    // Store the metadata that came with the response in case we need to pass it
    // along with the prefetch query to Instant.
    JSONStringValueSerializer json_serializer(&results->metadata);
    json_serializer.Serialize(*extras);
  }

  // Clear the previous results now that new results are available.
  results->suggest_results.clear();
  results->navigation_results.clear();

  base::string16 suggestion;
  std::string type;
  int relevance = GetDefaultResultRelevance();
  // Prohibit navsuggest in FORCED_QUERY mode.  Users wants queries, not URLs.
  const bool allow_navsuggest =
      input.type() != metrics::OmniboxInputType::FORCED_QUERY;
  const std::string languages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  const base::string16& trimmed_input =
      base::CollapseWhitespace(input.text(), false);
  for (size_t index = 0; results_list->GetString(index, &suggestion); ++index) {
    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    if (suggestion.empty())
      continue;

    // Apply valid suggested relevance scores; discard invalid lists.
    if (relevances != NULL && !relevances->GetInteger(index, &relevance))
      relevances = NULL;
    AutocompleteMatchType::Type match_type =
        AutocompleteMatchType::SEARCH_SUGGEST;
    if (types && types->GetString(index, &type))
      match_type = GetAutocompleteMatchType(type);
    const base::DictionaryValue* suggestion_detail = NULL;
    std::string deletion_url;

    if (suggestion_details &&
        suggestion_details->GetDictionary(index, &suggestion_detail))
      suggestion_detail->GetString("du", &deletion_url);

    if ((match_type == AutocompleteMatchType::NAVSUGGEST) ||
        (match_type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED)) {
      // Do not blindly trust the URL coming from the server to be valid.
      GURL url(
          url_fixer::FixupURL(base::UTF16ToUTF8(suggestion), std::string()));
      if (url.is_valid() && allow_navsuggest) {
        base::string16 title;
        if (descriptions != NULL)
          descriptions->GetString(index, &title);
        results->navigation_results.push_back(NavigationResult(
            ChromeAutocompleteSchemeClassifier(profile_), url, match_type,
            title, deletion_url, is_keyword_result, relevance,
            relevances != NULL, input.text(), languages));
      }
    } else {
      base::string16 match_contents = suggestion;
      base::string16 match_contents_prefix;
      base::string16 annotation;
      base::string16 answer_contents;
      base::string16 answer_type;
      std::string suggest_query_params;

      if (suggestion_details) {
        suggestion_details->GetDictionary(index, &suggestion_detail);
        if (suggestion_detail) {
          suggestion_detail->GetString("t", &match_contents);
          suggestion_detail->GetString("mp", &match_contents_prefix);
          // Error correction for bad data from server.
          if (match_contents.empty())
            match_contents = suggestion;
          suggestion_detail->GetString("a", &annotation);
          suggestion_detail->GetString("q", &suggest_query_params);

          // Extract Answers, if provided.
          const base::DictionaryValue* answer_json = NULL;
          if (suggestion_detail->GetDictionary("ansa", &answer_json)) {
            match_type = AutocompleteMatchType::SEARCH_SUGGEST_ANSWER;
            PrefetchAnswersImages(answer_json);
            std::string contents;
            base::JSONWriter::Write(answer_json, &contents);
            answer_contents = base::UTF8ToUTF16(contents);
            suggestion_detail->GetString("ansb", &answer_type);
          }
        }
      }

      bool should_prefetch = static_cast<int>(index) == prefetch_index;
      // TODO(kochi): Improve calculator suggestion presentation.
      results->suggest_results.push_back(SuggestResult(
          base::CollapseWhitespace(suggestion, false), match_type,
          base::CollapseWhitespace(match_contents, false),
          match_contents_prefix, annotation, answer_contents, answer_type,
          suggest_query_params, deletion_url, is_keyword_result, relevance,
          relevances != NULL, should_prefetch, trimmed_input));
    }
  }
  SortResults(is_keyword_result, relevances, results);
  return true;
}

void BaseSearchProvider::PrefetchAnswersImages(
    const base::DictionaryValue* answer_json) {
  DCHECK(answer_json);
  const base::ListValue* lines = NULL;
  answer_json->GetList("l", &lines);
  if (!lines || lines->GetSize() == 0)
    return;

  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  DCHECK(image_service);

  for (size_t line = 0; line < lines->GetSize(); ++line) {
    const base::DictionaryValue* imageLine = NULL;
    lines->GetDictionary(line, &imageLine);
    if (!imageLine)
      continue;
    const base::DictionaryValue* imageData = NULL;
    imageLine->GetDictionary("i", &imageData);
    if (!imageData)
      continue;
    std::string imageUrl;
    imageData->GetString("d", &imageUrl);
    image_service->Prefetch(GURL(imageUrl));
  }
}

void BaseSearchProvider::SortResults(bool is_keyword,
                                     const base::ListValue* relevances,
                                     Results* results) {
}

bool BaseSearchProvider::StoreSuggestionResponse(
    const std::string& json_data,
    const base::Value& parsed_data) {
  return false;
}

void BaseSearchProvider::ModifyProviderInfo(
    metrics::OmniboxEventProto_ProviderInfo* provider_info) const {
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
  SuggestionDeletionHandlers::iterator it = std::find(
      deletion_handlers_.begin(), deletion_handlers_.end(), handler);
  DCHECK(it != deletion_handlers_.end());
  deletion_handlers_.erase(it);
}
