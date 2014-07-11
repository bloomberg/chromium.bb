// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/url_search_provider.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/autocomplete/autocomplete_scheme_classifier.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "net/url_request/url_request.h"
#include "ui/app_list/search_result.h"
#include "url/gurl.h"

namespace athena {

namespace {

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

  DISALLOW_COPY_AND_ASSIGN(AthenaTemplateURLServiceClient);
};

class UrlSearchResult : public app_list::SearchResult {
 public:
  UrlSearchResult(content::BrowserContext* browser_context,
                  TemplateURLService* template_url_service,
                  const base::string16& query)
      : browser_context_(browser_context),
        template_url_service_(template_url_service),
        input_(query,
               base::string16::npos /* cursor_position */,
               base::string16() /* desired_tld */,
               GURL() /* current_url */,
               metrics::OmniboxEventProto::INVALID_SPEC,
               false /* prevent_inline_autocomplete */,
               false /* prefer_keyword */,
               true /* allow_extract_keyword_match */,
               true /* want_asynchronous_matches */,
               AthenaSchemeClassifier()) {
    set_title(query);
    app_list::SearchResult::Tags title_tags;
    if (input_.type() == metrics::OmniboxInputType::URL) {
      title_tags.push_back(app_list::SearchResult::Tag(
          app_list::SearchResult::Tag::URL, 0, query.size()));
    }
    title_tags.push_back(app_list::SearchResult::Tag(
        app_list::SearchResult::Tag::MATCH, 0, query.size()));
    set_title_tags(title_tags);
    set_id(base::UTF16ToUTF8(query));
  }

 private:
  // Overriddenn from app_list::SearchResult:
  virtual void Open(int event_flags) OVERRIDE {
    GURL url;
    if (input_.type() == metrics::OmniboxInputType::URL) {
      url = input_.canonicalized_url();
    } else {
      TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      const TemplateURLRef& search_url = template_url->url_ref();
      TemplateURLRef::SearchTermsArgs search_terms_args(input_.text());
      url = GURL(search_url.ReplaceSearchTerms(
          search_terms_args, template_url_service_->search_terms_data()));
    }
    ActivityManager::Get()->AddActivity(
        ActivityFactory::Get()->CreateWebActivity(browser_context_, url));
  }

  content::BrowserContext* browser_context_;
  TemplateURLService* template_url_service_;
  AutocompleteInput input_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchResult);
};

}  // namespace

UrlSearchProvider::UrlSearchProvider(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      // TODO(mukai): introduce the real parameters when it's necessary.
      template_url_service_(
          new TemplateURLService(NULL /* prefs */,
                                 make_scoped_ptr(new SearchTermsData()),
                                 NULL /* KeywordWebDataService */,
                                 scoped_ptr<TemplateURLServiceClient>(
                                     new AthenaTemplateURLServiceClient()),
                                 NULL /*GoogleURLTracker */,
                                 NULL /* RapporService */,
                                 base::Closure() /* dsp_change_callback */)) {
  template_url_service_->Load();
}

UrlSearchProvider::~UrlSearchProvider() {
}

void UrlSearchProvider::Start(const base::string16& query) {
  ClearResults();
  Add(scoped_ptr<app_list::SearchResult>(new UrlSearchResult(
      browser_context_, template_url_service_.get(), query)));
}

void UrlSearchProvider::Stop() {
}

}  // namespace athena
