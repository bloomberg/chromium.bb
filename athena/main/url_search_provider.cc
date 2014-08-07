// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/url_search_provider.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_scheme_classifier.h"
#include "components/omnibox/search_suggestion_parser.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/browser_context.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "ui/app_list/search_result.h"
#include "url/gurl.h"

namespace athena {

namespace {

// The SearchTermsData implementation for Athena.
class AthenaSearchTermsData : public SearchTermsData {
 public:
  // SearchTermsData:
  virtual std::string GetSuggestClient() const OVERRIDE {
    return "chrome";
  }
};

// The AutocompleteSchemeClassifier implementation for Athena.
// TODO(mukai): Introduce supports of normal schemes like about: or blob:
class AthenaSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  AthenaSchemeClassifier() {}

  // AutocompleteSchemeClassifier:
  virtual metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const OVERRIDE {
    if (net::URLRequest::IsHandledProtocol(scheme))
      return metrics::OmniboxInputType::URL;
    return metrics::OmniboxInputType::INVALID;
  }
};

// The templateURLServiceClient for Athena. Mainly for the interaction with
// history module (see chrome/browser/search_engines for Chrome implementation).
// TODO(mukai): Implement the contents of this class when it's necessary.
class AthenaTemplateURLServiceClient : public TemplateURLServiceClient {
 public:
  AthenaTemplateURLServiceClient() {}
  virtual ~AthenaTemplateURLServiceClient() {}

 private:
  // TemplateURLServiceClient:
  virtual void SetOwner(TemplateURLService* owner) OVERRIDE {}
  virtual void DeleteAllSearchTermsForKeyword(TemplateURLID id) OVERRIDE {}
  virtual void SetKeywordSearchTermsForURL(
      const GURL& url,
      TemplateURLID id,
      const base::string16& term) OVERRIDE {}
  virtual void AddKeywordGeneratedVisit(const GURL& url) OVERRIDE {}
  virtual void RestoreExtensionInfoIfNecessary(
      TemplateURL* template_url) OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(AthenaTemplateURLServiceClient);
};

class UrlSearchResult : public app_list::SearchResult {
 public:
  UrlSearchResult(content::BrowserContext* browser_context,
                  const GURL& url,
                  const base::string16& title)
      : browser_context_(browser_context),
        url_(url) {
    set_title(title);
    set_id(url_.spec());
  }

 private:
  // Overriddenn from app_list::SearchResult:
  virtual void Open(int event_flags) OVERRIDE {
    ActivityManager::Get()->AddActivity(
        ActivityFactory::Get()->CreateWebActivity(browser_context_, url_));
  }

  content::BrowserContext* browser_context_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchResult);
};

scoped_ptr<app_list::SearchResult> CreateResultForSearchQuery(
    content::BrowserContext* browser_context,
    TemplateURLService* template_url_service,
    const base::string16& search_query) {
  TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  const TemplateURLRef& search_url = template_url->url_ref();
  TemplateURLRef::SearchTermsArgs search_terms_args(search_query);
  return scoped_ptr<app_list::SearchResult>(new UrlSearchResult(
      browser_context,
      GURL(search_url.ReplaceSearchTerms(
          search_terms_args, template_url_service->search_terms_data())),
      search_query));
}

scoped_ptr<app_list::SearchResult> CreateResultForInput(
    content::BrowserContext* browser_context,
    TemplateURLService* template_url_service,
    const AutocompleteInput& input) {
  scoped_ptr<app_list::SearchResult> result;
  app_list::SearchResult::Tags title_tags;
  if (input.type() == metrics::OmniboxInputType::URL) {
    result.reset(new UrlSearchResult(
        browser_context, input.canonicalized_url(), input.text()));
    title_tags.push_back(app_list::SearchResult::Tag(
        app_list::SearchResult::Tag::URL, 0, input.text().size()));
  } else {
    result = CreateResultForSearchQuery(
        browser_context, template_url_service, input.text());
  }
  title_tags.push_back(app_list::SearchResult::Tag(
      app_list::SearchResult::Tag::MATCH, 0, input.text().size()));
  result->set_title_tags(title_tags);
  return result.Pass();
}

}  // namespace

UrlSearchProvider::UrlSearchProvider(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      // TODO(mukai): introduce the real parameters when it's necessary.
      template_url_service_(
          new TemplateURLService(NULL /* prefs */,
                                 scoped_ptr<SearchTermsData>(
                                     new AthenaSearchTermsData()),
                                 NULL /* KeywordWebDataService */,
                                 scoped_ptr<TemplateURLServiceClient>(
                                     new AthenaTemplateURLServiceClient()),
                                 NULL /*GoogleURLTracker */,
                                 NULL /* RapporService */,
                                 base::Closure() /* dsp_change_callback */)),
      should_fetch_suggestions_again_(false) {
  template_url_service_->Load();
}

UrlSearchProvider::~UrlSearchProvider() {
}

void UrlSearchProvider::Start(const base::string16& query) {
  input_ = AutocompleteInput(query,
                             base::string16::npos /* cursor_position */,
                             base::string16() /* desired_tld */,
                             GURL() /* current_url */,
                             metrics::OmniboxEventProto::INVALID_SPEC,
                             false /* prevent_inline_autocomplete */,
                             false /* prefer_keyword */,
                             true /* allow_extract_keyword_match */,
                             true /* want_asynchronous_matches */,
                             AthenaSchemeClassifier());
  ClearResults();
  Add(CreateResultForInput(browser_context_, template_url_service_.get(),
                           input_));
  StartFetchingSuggestions();
}

void UrlSearchProvider::Stop() {
  suggestion_fetcher_.reset();
}

void UrlSearchProvider::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(suggestion_fetcher_.get(), source);

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string json_data = SearchSuggestionParser::ExtractJsonData(source);
    scoped_ptr<base::Value> data(
        SearchSuggestionParser::DeserializeJsonData(json_data));
    if (data) {
      const int kDefaultRelevance = 0;
      SearchSuggestionParser::Results results;
      if (SearchSuggestionParser::ParseSuggestResults(
              *data, input_, AthenaSchemeClassifier(), kDefaultRelevance,
              std::string(),  // languages
              false,  // is_keyword_result
              &results)) {
        ClearResults();
        Add(CreateResultForInput(browser_context_, template_url_service_.get(),
                                 input_));
        for (size_t i = 0; i < results.suggest_results.size(); ++i) {
          const SearchSuggestionParser::SuggestResult& result =
              results.suggest_results[i];
          Add(CreateResultForSearchQuery(browser_context_,
                                         template_url_service_.get(),
                                         result.suggestion()));
        }
        for (size_t i = 0; i < results.navigation_results.size(); ++i) {
          const SearchSuggestionParser::NavigationResult& result =
              results.navigation_results[i];
          Add(scoped_ptr<app_list::SearchResult>(
              new UrlSearchResult(browser_context_, result.url(),
                                  result.description())));
        }
      }
    }
  }
  suggestion_fetcher_.reset();
  if (should_fetch_suggestions_again_)
    StartFetchingSuggestions();
}

void UrlSearchProvider::StartFetchingSuggestions() {
  if (suggestion_fetcher_) {
    should_fetch_suggestions_again_ = true;
    return;
  }
  should_fetch_suggestions_again_ = false;

  // Bail if the suggestion URL is invalid with the given replacements.
  TemplateURL* template_url = template_url_service_->GetDefaultSearchProvider();
  TemplateURLRef::SearchTermsArgs search_term_args(input_.text());
  search_term_args.input_type = input_.type();
  search_term_args.cursor_position = input_.cursor_position();
  search_term_args.page_classification = input_.current_page_classification();
  GURL suggest_url(template_url->suggestions_url_ref().ReplaceSearchTerms(
      search_term_args, template_url_service_->search_terms_data()));
  if (!suggest_url.is_valid()) {
    DLOG(ERROR) << "Invalid URL: " << suggest_url;
    return;
  }
  suggestion_fetcher_.reset(
      net::URLFetcher::Create(suggest_url, net::URLFetcher::GET, this));
  suggestion_fetcher_->SetRequestContext(browser_context_->GetRequestContext());
  suggestion_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  suggestion_fetcher_->Start();
}

}  // namespace athena
