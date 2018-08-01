// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace {
// TODO(skare): Pull the enum in search_provider.cc into its .h file, and switch
// this file and zero_suggest_provider.cc to use it.
enum DocumentRequestsHistogramValue {
  DOCUMENT_REQUEST_SENT = 1,
  DOCUMENT_REQUEST_INVALIDATED = 2,
  DOCUMENT_REPLY_RECEIVED = 3,
  DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxDocumentRequest(DocumentRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.DocumentSuggest.Requests", request_value,
                            DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE);
}

}  // namespace

// static
DocumentProvider* DocumentProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new DocumentProvider(client, listener);
}

// static
void DocumentProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(omnibox::kDocumentSuggestEnabled, true);
}

bool DocumentProvider::IsDocumentProviderAllowed(
    PrefService* prefs,
    bool is_incognito,
    bool is_authenticated,
    const TemplateURLService* template_url_service) {
  // Feature must be on.
  if (!base::FeatureList::IsEnabled(omnibox::kDocumentProvider))
    return false;

  // Client-side toggle must be enabled.
  if (!prefs->GetBoolean(omnibox::kDocumentSuggestEnabled))
    return false;

  // No incognito.
  if (is_incognito)
    return false;

  // User must be signed in.
  if (!is_authenticated)
    return false;

  // Google must be set as default search provider; we mix results which may
  // change placement.
  if (template_url_service == nullptr)
    return false;
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider != nullptr &&
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

void DocumentProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Start");
  matches_.clear();

  // TODO(skare): get from identitymanager, to short-circuit sooner.
  bool is_authenticated = true;
  if (!IsDocumentProviderAllowed(client_->GetPrefs(), client_->IsOffTheRecord(),
                                 is_authenticated,
                                 client_->GetTemplateURLService())) {
    return;
  }

  // Experiment: don't issue queries for inputs under some length.
  const size_t min_query_length =
      static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
          omnibox::kDocumentProvider, "DocumentProviderMinQueryLength", 4));
  if (input.text().length() < min_query_length) {
    return;
  }

  // We currently only provide asynchronous matches.
  if (!input.want_asynchronous_matches()) {
    return;
  }

  Stop(true, false);

  // Create a request for suggestions, routing completion to
  base::BindOnce(&DocumentProvider::OnDocumentSuggestionsLoaderAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DocumentProvider::OnURLLoadComplete,
                     base::Unretained(this) /* own SimpleURLLoader */);

  done_ = false;  // Set true in callbacks.
  client_->GetDocumentSuggestionsService(/*create_if_necessary=*/true)
      ->CreateDocumentSuggestionsRequest(
          input.text(), client_->GetTemplateURLService(),
          base::BindOnce(
              &DocumentProvider::OnDocumentSuggestionsLoaderAvailable,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &DocumentProvider::OnURLLoadComplete,
              base::Unretained(this) /* this owns SimpleURLLoader */));
}

void DocumentProvider::Stop(bool clear_cached_results,
                            bool due_to_user_inactivity) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Stop");
  if (loader_)
    LogOmniboxDocumentRequest(DOCUMENT_REQUEST_INVALIDATED);
  loader_.reset();
  auto* document_suggestions_service =
      client_->GetDocumentSuggestionsService(/*create_if_necessary=*/false);
  if (document_suggestions_service != nullptr) {
    document_suggestions_service->StopCreatingDocumentSuggestionsRequest();
  }

  done_ = true;

  if (clear_cached_results) {
    matches_.clear();
  }
}

void DocumentProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Not supported by this provider.
  return;
}

void DocumentProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  // TODO(skare): Verify that we don't lose metrics based on what
  // zero_suggest_provider and BaseSearchProvider add.
  return;
}

DocumentProvider::DocumentProvider(AutocompleteProviderClient* client,
                                   AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_DOCUMENT),
      client_(client),
      listener_(listener),
      weak_ptr_factory_(this) {}

DocumentProvider::~DocumentProvider() {}

void DocumentProvider::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogOmniboxDocumentRequest(DOCUMENT_REPLY_RECEIVED);

  const bool results_updated =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200) &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  loader_.reset();
  done_ = true;
  listener_->OnProviderUpdate(results_updated);
}

bool DocumentProvider::UpdateResults(const std::string& json_data) {
  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(base::JSONReader::Read(json_data));
  if (!response)
    return false;

  return ParseDocumentSearchResults(*response, &matches_);
}

void DocumentProvider::OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  loader_ = std::move(loader);
  LogOmniboxDocumentRequest(DOCUMENT_REQUEST_SENT);
}

bool DocumentProvider::ParseDocumentSearchResults(const base::Value& root_val,
                                                  ACMatches* matches) {
  const base::DictionaryValue* root_dict = nullptr;
  const base::ListValue* results_list = nullptr;
  if (!root_val.GetAsDictionary(&root_dict)) {
    return false;
  }
  if (!root_dict->GetList("results", &results_list)) {
    return false;
  }
  size_t num_results = results_list->GetSize();
  UMA_HISTOGRAM_COUNTS("Omnibox.DocumentSuggest.ResultCount", num_results);

  // Create a synthetic score. Eventually we'll have signals from the API.
  // For now, allow setting of each of three scores from Finch.
  int score0 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult1", 1100);
  int score1 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult2", 700);
  int score2 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult3", 300);

  // Clear the previous results now that new results are available.
  matches->clear();
  for (size_t i = 0; i < num_results; i++) {
    if (matches->size() >= AutocompleteProvider::kMaxMatches) {
      break;
    }
    const base::DictionaryValue* result = nullptr;
    if (!results_list->GetDictionary(i, &result)) {
      return false;
    }
    base::string16 title;
    base::string16 url;
    result->GetString("title", &title);
    result->GetString("url", &url);
    if (title.empty() || url.empty()) {
      continue;
    }
    int relevance = 0;
    switch (matches->size()) {
      case 0:
        relevance = score0;
        break;
      case 1:
        relevance = score1;
        break;
      case 2:
        relevance = score2;
        break;
      default:
        break;
    }
    AutocompleteMatch match(this, relevance, false,
                            AutocompleteMatchType::DOCUMENT_SUGGESTION);
    match.destination_url = GURL(url);
    match.contents = title;
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    matches->push_back(match);
  }
  return true;
}
