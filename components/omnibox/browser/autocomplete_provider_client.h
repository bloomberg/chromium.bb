// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/top_sites.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/shortcuts_backend.h"

class AutocompleteController;
struct AutocompleteMatch;
class AutocompleteClassifier;
class AutocompleteSchemeClassifier;
class GURL;
class InMemoryURLIndex;
class KeywordProvider;
class PrefService;
class ShortcutsBackend;

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

namespace physical_web {
class PhysicalWebDataSource;
}

class SearchTermsData;
class TemplateURLService;

class AutocompleteProviderClient {
 public:
  virtual ~AutocompleteProviderClient() {}

  virtual net::URLRequestContextGetter* GetRequestContext() = 0;
  virtual PrefService* GetPrefs() = 0;
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual scoped_refptr<history::TopSites> GetTopSites() = 0;
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::URLDatabase* GetInMemoryDatabase() = 0;
  virtual InMemoryURLIndex* GetInMemoryURLIndex() = 0;
  virtual TemplateURLService* GetTemplateURLService() = 0;
  virtual const TemplateURLService* GetTemplateURLService() const = 0;
  virtual const SearchTermsData& GetSearchTermsData() const = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackend() = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() = 0;
  virtual std::unique_ptr<KeywordExtensionsDelegate>
  GetKeywordExtensionsDelegate(KeywordProvider* keyword_provider) = 0;
  virtual physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() = 0;

  // The value to use for Accept-Languages HTTP header when making an HTTP
  // request.
  virtual std::string GetAcceptLanguages() const = 0;

  // The embedder's representation of the |about| URL scheme for builtin URLs
  // (e.g., |chrome| for Chrome).
  virtual std::string GetEmbedderRepresentationOfAboutScheme() = 0;

  // The set of built-in URLs considered worth suggesting as autocomplete
  // suggestions to the user.  Some built-in URLs, e.g. hidden URLs that
  // intentionally crash the product for testing purposes, may be omitted from
  // this list if suggesting them is undesirable.
  virtual std::vector<base::string16> GetBuiltinURLs() = 0;

  // The set of URLs to provide as autocomplete suggestions as the user types a
  // prefix of the |about| scheme or the embedder's representation of that
  // scheme. Note that this may be a subset of GetBuiltinURLs(), e.g., only the
  // most commonly-used URLs from that set.
  virtual std::vector<base::string16> GetBuiltinsToProvideAsUserTypes() = 0;

  virtual bool IsOffTheRecord() const = 0;
  virtual bool SearchSuggestEnabled() const = 0;

  virtual bool TabSyncEnabledAndUnencrypted() const = 0;

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

  // Sends a hint to the service worker context that navigation to
  // |desination_url| is likely. On platforms where this is supported, the
  // service worker lookup can be expensive so this method should only be
  // called once per input session.
  virtual void StartServiceWorker(const GURL& destination_url) {}

  // Called by |controller| when its results have changed and all providers are
  // done processing the autocomplete request. At the //chrome level, this
  // callback results in firing the
  // NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY notification.
  // TODO(blundell): Remove the //chrome-level notification entirely in favor of
  // having AutocompleteController expose a CallbackList that //chrome-level
  // listeners add themselves to, and then kill this method.
  virtual void OnAutocompleteControllerResultReady(
      AutocompleteController* controller) {}

  // Called after creation of |keyword_provider| to allow the client to
  // configure the provider if desired.
  virtual void ConfigureKeywordProvider(KeywordProvider* keyword_provider) {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
