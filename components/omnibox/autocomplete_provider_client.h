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

namespace history {
class URLDatabase;
}

namespace net {
class URLRequestContextGetter;
}

class AutocompleteProviderClient {
 public:
  virtual ~AutocompleteProviderClient() {}

  // Returns the request context.
  virtual net::URLRequestContextGetter* RequestContext() = 0;

  // Returns whether the provider should work in incognito mode.
  virtual bool IsOffTheRecord() = 0;

  // The value to use for Accept-Languages HTTP header when making an HTTP
  // request.
  virtual std::string AcceptLanguages() = 0;

  // Returns true when suggest support is enabled.
  virtual bool SearchSuggestEnabled() = 0;

  // Returns whether the bookmark bar is visible on all tabs.
  virtual bool ShowBookmarkBar() = 0;

  // Returns the scheme classifier.
  virtual const AutocompleteSchemeClassifier& SchemeClassifier() = 0;

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.
  virtual void Classify(
      const base::string16& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) = 0;

  // Returns the in-memory URL database.
  virtual history::URLDatabase* InMemoryDatabase() = 0;

  // Deletes all URL and search term entries matching the given |term| and
  // |keyword_id| from history.
  virtual void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const base::string16& term) = 0;

  // Returns whether the user has tab sync enabled and tab sync is unencrypted.
  virtual bool TabSyncEnabledAndUnencrypted() = 0;

  // Starts prefetching the image at the given |url|.
  virtual void PrefetchImage(const GURL& url) = 0;
};

#endif  // COMPONENTS_OMNIBOX_AUTOCOMPLETE_PROVIDER_CLIENT_H_
