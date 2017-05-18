// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CONTENT_SUGGESTED_ARTICLES_OBSERVER_H_
#define COMPONENTS_OFFLINE_PAGES_CONTENT_SUGGESTED_ARTICLES_OBSERVER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/content_suggestions_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ntp_snippets {
class Category;
}

namespace offline_pages {
class PrefetchService;

// Observes the ContentSuggestionsService, listening for new suggestions in the
// ARTICLES category.  When those suggestions arrive, it then forwards them to
// the Prefetch Service, which does not know about Content Suggestions
// specifically.
class SuggestedArticlesObserver
    : public ntp_snippets::ContentSuggestionsService::Observer,
      public base::SupportsUserData::Data {
 public:
  // Delegate exists to allow for dependency injection in unit tests.
  // SuggestedArticlesObserver implements its own delegate, |DefaultDelegate| in
  // the .cc file that forwards to the ContentSuggestionsService and the
  // PrefetchServiceFactory.  Code inside |DefaultDelegate| should be as simple
  // as possible, since it will only be covered by instrumentation/browser
  // tests.
  class Delegate {
   public:
    virtual const std::vector<ntp_snippets::ContentSuggestion>& GetSuggestions(
        const ntp_snippets::Category& category) = 0;
    virtual PrefetchService* GetPrefetchService(
        content::BrowserContext* context) = 0;
    virtual ~Delegate() = default;
  };

  // This API creates a new SuggestedArticlesObserver and adds it as an
  // observer to the ContentSuggestionsService provided.  Its lifetime is
  // managed by the ContentSuggestionsService.
  static void ObserveContentSuggestionsService(
      content::BrowserContext* browser_context,
      ntp_snippets::ContentSuggestionsService* service);

  SuggestedArticlesObserver(content::BrowserContext* browser_context,
                            std::unique_ptr<Delegate> delegate);
  ~SuggestedArticlesObserver() override;

  // ContentSuggestionsService::Observer overrides.
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void OnFullRefreshRequired() override;
  void ContentSuggestionsServiceShutdown() override;

 private:
  content::BrowserContext* browser_context_;
  ntp_snippets::CategoryStatus category_status_ =
      ntp_snippets::CategoryStatus::INITIALIZING;
  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedArticlesObserver);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CONTENT_SUGGESTED_ARTICLES_OBSERVER_H_
