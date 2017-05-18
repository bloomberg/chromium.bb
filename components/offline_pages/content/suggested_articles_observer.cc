// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/content/suggested_articles_observer.h"

#include <unordered_set>

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/offline_pages/content/prefetch_service_factory.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

using ntp_snippets::Category;
using ntp_snippets::ContentSuggestion;

namespace offline_pages {

namespace {

int kOfflinePageSuggestedArticlesObserverUserDataKey;

const ntp_snippets::Category& ArticlesCategory() {
  static ntp_snippets::Category articles =
      Category::FromKnownCategory(ntp_snippets::KnownCategories::ARTICLES);
  return articles;
}

ClientId CreateClientIDFromSuggestionId(const ContentSuggestion::ID& id) {
  return ClientId(kSuggestedArticlesNamespace, id.id_within_category());
}

// The default delegate that contains external dependencies for the Offline Page
// Suggestions Observer.  This is unused in tests, which implement their own
// Delegate.
class DefaultDelegate : public SuggestedArticlesObserver::Delegate {
 public:
  explicit DefaultDelegate(ntp_snippets::ContentSuggestionsService* service);
  ~DefaultDelegate() override = default;

  const std::vector<ContentSuggestion>& GetSuggestions(
      const Category& category) override;
  PrefetchService* GetPrefetchService(
      content::BrowserContext* context) override;

 private:
  ntp_snippets::ContentSuggestionsService* service_;
};

DefaultDelegate::DefaultDelegate(
    ntp_snippets::ContentSuggestionsService* service)
    : service_(service) {}

const std::vector<ContentSuggestion>& DefaultDelegate::GetSuggestions(
    const Category& category) {
  return service_->GetSuggestionsForCategory(category);
}

PrefetchService* DefaultDelegate::GetPrefetchService(
    content::BrowserContext* context) {
  return PrefetchServiceFactory::GetForBrowserContext(context);
}

}  // namespace

// static
void SuggestedArticlesObserver::ObserveContentSuggestionsService(
    content::BrowserContext* browser_context,
    ntp_snippets::ContentSuggestionsService* service) {
  if (!offline_pages::IsPrefetchingOfflinePagesEnabled())
    return;

  auto suggestions_observer = base::MakeUnique<SuggestedArticlesObserver>(
      browser_context, base::MakeUnique<DefaultDelegate>(service));
  service->AddObserver(suggestions_observer.get());
  service->SetUserData(&kOfflinePageSuggestedArticlesObserverUserDataKey,
                       std::move(suggestions_observer));
}

SuggestedArticlesObserver::SuggestedArticlesObserver(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : browser_context_(browser_context), delegate_(std::move(delegate)) {}

SuggestedArticlesObserver::~SuggestedArticlesObserver() = default;

void SuggestedArticlesObserver::OnNewSuggestions(Category category) {
  // TODO(dewittj): Change this to check whether a given category is not
  // a _remote_ category.
  if (category != ArticlesCategory() ||
      category_status_ != ntp_snippets::CategoryStatus::AVAILABLE) {
    return;
  }

  const std::vector<ContentSuggestion>& suggestions =
      delegate_->GetSuggestions(ArticlesCategory());
  if (suggestions.empty())
    return;

  std::vector<PrefetchDispatcher::PrefetchURL> prefetch_urls;
  for (const ContentSuggestion& suggestion : suggestions) {
    prefetch_urls.push_back(
        {CreateClientIDFromSuggestionId(suggestion.id()), suggestion.url()});
  }

  PrefetchService* service = delegate_->GetPrefetchService(browser_context_);
  if (service == nullptr) {
    DVLOG(1) << "PrefetchService unavailable to the "
                "SuggestedArticlesObserver.";
    return;
  }
  service->GetDispatcher()->AddCandidatePrefetchURLs(prefetch_urls);
}

void SuggestedArticlesObserver::OnCategoryStatusChanged(
    Category category,
    ntp_snippets::CategoryStatus new_status) {
  if (category != ArticlesCategory() || category_status_ == new_status)
    return;

  category_status_ = new_status;

  if (category_status_ ==
          ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED ||
      category_status_ ==
          ntp_snippets::CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED) {
    PrefetchService* service = delegate_->GetPrefetchService(browser_context_);
    if (service == nullptr) {
      DVLOG(1) << "PrefetchService unavailable to the "
                  "SuggestedArticlesObserver.";
      return;
    }
    service->GetDispatcher()->RemoveAllUnprocessedPrefetchURLs(
        kSuggestedArticlesNamespace);
  }
}

void SuggestedArticlesObserver::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  PrefetchService* service = delegate_->GetPrefetchService(browser_context_);
  if (service == nullptr) {
    DVLOG(1) << "PrefetchService unavailable to the "
                "SuggestedArticlesObserver.";
    return;
  }
  service->GetDispatcher()->RemovePrefetchURLsByClientId(
      CreateClientIDFromSuggestionId(suggestion_id));
}

void SuggestedArticlesObserver::OnFullRefreshRequired() {
  PrefetchService* service = delegate_->GetPrefetchService(browser_context_);
  if (service == nullptr) {
    DVLOG(1) << "PrefetchService unavailable to the "
                "SuggestedArticlesObserver.";
    return;
  }
  service->GetDispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  OnNewSuggestions(ArticlesCategory());
}

void SuggestedArticlesObserver::ContentSuggestionsServiceShutdown() {
  // No need to do anything here, we will just stop getting events.
}

}  // namespace offline_pages
