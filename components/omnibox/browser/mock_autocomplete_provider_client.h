// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <string>
#include <utility>

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"

struct AutocompleteMatch;

class MockAutocompleteProviderClient : public AutocompleteProviderClient {
 public:
  MockAutocompleteProviderClient();
  ~MockAutocompleteProviderClient();

  // AutocompleteProviderClient:
  MOCK_METHOD0(GetRequestContext, net::URLRequestContextGetter*());
  MOCK_METHOD0(GetPrefs, PrefService*());
  MOCK_CONST_METHOD0(GetSchemeClassifier,
                     const AutocompleteSchemeClassifier&());
  MOCK_METHOD0(GetAutocompleteClassifier, AutocompleteClassifier*());
  MOCK_METHOD0(GetHistoryService, history::HistoryService*());

  // Can't mock scoped_refptr :\.
  scoped_refptr<history::TopSites> GetTopSites() override { return nullptr; }

  MOCK_METHOD0(GetBookmarkModel, bookmarks::BookmarkModel*());
  MOCK_METHOD0(GetInMemoryDatabase, history::URLDatabase*());
  MOCK_METHOD0(GetInMemoryURLIndex, InMemoryURLIndex*());

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }
  const TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }
  std::unique_ptr<KeywordExtensionsDelegate> GetKeywordExtensionsDelegate(
      KeywordProvider* keyword_provider) override {
    return nullptr;
  }
  physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() override {
    return nullptr;
  }

  MOCK_CONST_METHOD0(GetSearchTermsData, const SearchTermsData&());

  // Can't mock scoped_refptr :\.
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override {
    return nullptr;
  }
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override {
    return nullptr;
  }

  MOCK_CONST_METHOD0(GetAcceptLanguages, std::string());
  MOCK_METHOD0(GetEmbedderRepresentationOfAboutScheme, std::string());
  MOCK_METHOD0(GetBuiltinURLs, std::vector<base::string16>());
  MOCK_METHOD0(GetBuiltinsToProvideAsUserTypes, std::vector<base::string16>());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_CONST_METHOD0(SearchSuggestEnabled, bool());
  MOCK_CONST_METHOD0(TabSyncEnabledAndUnencrypted, bool());
  MOCK_METHOD6(
      Classify,
      void(const base::string16& text,
           bool prefer_keyword,
           bool allow_exact_keyword_match,
           metrics::OmniboxEventProto::PageClassification page_classification,
           AutocompleteMatch* match,
           GURL* alternate_nav_url));
  MOCK_METHOD2(DeleteMatchingURLsForKeywordFromHistory,
               void(history::KeywordID keyword_id, const base::string16& term));
  MOCK_METHOD1(PrefetchImage, void(const GURL& url));

  void set_template_url_service(std::unique_ptr<TemplateURLService> service) {
    template_url_service_ = std::move(service);
  }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(MockAutocompleteProviderClient);
};

#endif  // COMPONENTS_OMNIBOX_AUTOCOMPLETE_PROVIDER_CLIENT_H_
