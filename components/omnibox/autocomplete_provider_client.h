// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include "base/strings/string16.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/metrics/proto/omnibox_event.pb.h"

struct AutocompleteMatch;
class AutocompleteSchemeClassifier;
class GURL;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryService;
class URLDatabase;
}

namespace net {
class URLRequestContextGetter;
}

class SearchTermsData;
class TemplateURLService;

class AutocompleteProviderClient {
 public:
  virtual ~AutocompleteProviderClient() {}

  virtual net::URLRequestContextGetter* GetRequestContext() = 0;
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::URLDatabase* GetInMemoryDatabase() = 0;
  virtual TemplateURLService* GetTemplateURLService() = 0;
  virtual const SearchTermsData& GetSearchTermsData() = 0;

  // The value to use for Accept-Languages HTTP header when making an HTTP
  // request.
  virtual std::string GetAcceptLanguages() = 0;

  virtual bool IsOffTheRecord() = 0;
  virtual bool SearchSuggestEnabled() = 0;

  // Returns whether the bookmark bar is visible on all tabs.
  virtual bool ShowBookmarkBar() = 0;

  virtual bool TabSyncEnabledAndUnencrypted() = 0;

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.
  virtual void Classify(
      const base::string16& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) = 0;

  // Deletes all URL and search term entries matching the given |term| and
  // |keyword_id| from history.
  virtual void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const base::string16& term) = 0;

  virtual void PrefetchImage(const GURL& url) = 0;
};

#endif  // COMPONENTS_OMNIBOX_AUTOCOMPLETE_PROVIDER_CLIENT_H_
