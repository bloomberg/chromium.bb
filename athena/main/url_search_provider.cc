// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/url_search_provider.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/content/public/scheme_classifier_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_provider_client.h"
#include "components/omnibox/search_provider.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/browser_context.h"
#include "ui/app_list/search_result.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace athena {

namespace {

// This constant was copied from HistoryURLProvider.
// TODO(hashimoto): Componentize HistoryURLProvider and delete this.
const int kScoreForWhatYouTypedResult = 1203;

// The SearchTermsData implementation for Athena.
class AthenaSearchTermsData : public SearchTermsData {
 public:
  // SearchTermsData:
  virtual std::string GetSuggestClient() const OVERRIDE {
    return "chrome";
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

// The AutocompleteProviderClient for Athena.
class AthenaAutocompleteProviderClient : public AutocompleteProviderClient {
 public:
  explicit AthenaAutocompleteProviderClient(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context),
        scheme_classifier_(CreateSchemeClassifier(browser_context)) {}
  virtual ~AthenaAutocompleteProviderClient() {}

  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE {
    return browser_context_->GetRequestContext();
  }
  virtual bool IsOffTheRecord() OVERRIDE {
    return browser_context_->IsOffTheRecord();
  }
  virtual std::string AcceptLanguages() OVERRIDE {
    // TODO(hashimoto): Return the value stored in the prefs.
    return "en-US";
  }
  virtual bool SearchSuggestEnabled() OVERRIDE { return true; }
  virtual bool ShowBookmarkBar() OVERRIDE { return false; }
  virtual const AutocompleteSchemeClassifier& SchemeClassifier() OVERRIDE {
    return *scheme_classifier_;
  }
  virtual void Classify(
      const base::string16& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) OVERRIDE {}
  virtual history::URLDatabase* InMemoryDatabase() OVERRIDE { return NULL; }
  virtual void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const base::string16& term) OVERRIDE {}
  virtual bool TabSyncEnabledAndUnencrypted() OVERRIDE { return false; }
  virtual void PrefetchImage(const GURL& url) OVERRIDE {}

 private:
  content::BrowserContext* browser_context_;
  scoped_ptr<AutocompleteSchemeClassifier> scheme_classifier_;

  DISALLOW_COPY_AND_ASSIGN(AthenaAutocompleteProviderClient);
};

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= app_list::SearchResult::Tag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= app_list::SearchResult::Tag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= app_list::SearchResult::Tag::DIM;

  return tag_styles;
}

// Translates ACMatchClassifications into SearchResult tags.
void ACMatchClassificationsToTags(
    const base::string16& text,
    const ACMatchClassifications& text_classes,
    app_list::SearchResult::Tags* tags) {
  int tag_styles = app_list::SearchResult::Tag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != app_list::SearchResult::Tag::NONE) {
      tags->push_back(app_list::SearchResult::Tag(
          tag_styles, tag_start, text_class.offset));
      tag_styles = app_list::SearchResult::Tag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != app_list::SearchResult::Tag::NONE) {
    tags->push_back(app_list::SearchResult::Tag(
        tag_styles, tag_start, text.length()));
  }
}

class UrlSearchResult : public app_list::SearchResult {
 public:
  UrlSearchResult(content::BrowserContext* browser_context,
                  const AutocompleteMatch& match)
      : browser_context_(browser_context),
        match_(match) {
    set_id(match_.destination_url.spec());

    // Derive relevance from omnibox relevance and normalize it to [0, 1].
    // The magic number 1500 is the highest score of an omnibox result.
    // See comments in autocomplete_provider.h.
    set_relevance(match_.relevance / 1500.0);

    UpdateIcon();
    UpdateTitleAndDetails();
  }

  virtual ~UrlSearchResult() {}

 private:
  // Overriddenn from app_list::SearchResult:
  virtual void Open(int event_flags) OVERRIDE {
    Activity* activity = ActivityFactory::Get()->CreateWebActivity(
        browser_context_, base::string16(), match_.destination_url);
    Activity::Show(activity);
  }

  void UpdateIcon() {
    SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        AutocompleteMatch::TypeToIcon(match_.type)));
  }

  void UpdateTitleAndDetails() {
    set_title(match_.contents);
    SearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents,
                                 match_.contents_class,
                                 &title_tags);
    set_title_tags(title_tags);

    set_details(match_.description);
    SearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.description,
                                 match_.description_class,
                                 &details_tags);
    set_details_tags(details_tags);
  }

  content::BrowserContext* browser_context_;
  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(UrlSearchResult);
};

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
      provider_(new ::SearchProvider(
          this,
          template_url_service_.get(),
          scoped_ptr<AutocompleteProviderClient>(
              new AthenaAutocompleteProviderClient(browser_context_)))) {
  template_url_service_->Load();
}

UrlSearchProvider::~UrlSearchProvider() {
}

void UrlSearchProvider::Start(const base::string16& query) {
  const bool minimal_changes = query == input_.text();
  scoped_ptr<AutocompleteSchemeClassifier> scheme_classifier(
      CreateSchemeClassifier(browser_context_));
  input_ = AutocompleteInput(query,
                             base::string16::npos /* cursor_position */,
                             base::string16() /* desired_tld */,
                             GURL() /* current_url */,
                             metrics::OmniboxEventProto::INVALID_SPEC,
                             false /* prevent_inline_autocomplete */,
                             false /* prefer_keyword */,
                             true /* allow_extract_keyword_match */,
                             true /* want_asynchronous_matches */,
                             *scheme_classifier);

  // Clearing results here may cause unexpected results.
  // TODO(mukai): fix this by fixing crbug.com/415500
  if (!minimal_changes)
    ClearResults();

  if (input_.type() == metrics::OmniboxInputType::URL) {
    // TODO(hashimoto): Componentize HistoryURLProvider and remove this code.
    AutocompleteMatch what_you_typed_match(
        NULL, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    what_you_typed_match.destination_url = input_.canonicalized_url();
    what_you_typed_match.contents = input_.text();
    what_you_typed_match.relevance = kScoreForWhatYouTypedResult;
    Add(scoped_ptr<app_list::SearchResult>(new UrlSearchResult(
        browser_context_, what_you_typed_match)));
  }

  provider_->Start(input_, minimal_changes);
}

void UrlSearchProvider::Stop() {
  provider_->Stop(false);
}

void UrlSearchProvider::OnProviderUpdate(bool updated_matches) {
  if (!updated_matches)
    return;

  const ACMatches& matches = provider_->matches();
  for (ACMatches::const_iterator it = matches.begin(); it != matches.end();
       ++it) {
    if (!it->destination_url.is_valid())
      continue;

    Add(scoped_ptr<app_list::SearchResult>(new UrlSearchResult(
        browser_context_, *it)));
  }
}

}  // namespace athena
