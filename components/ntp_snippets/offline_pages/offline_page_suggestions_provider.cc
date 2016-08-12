// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"

using offline_pages::MultipleOfflinePageItemResult;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageItem;

namespace ntp_snippets {

namespace {

const int kMaxSuggestionsCount = 5;

}  // namespace

OfflinePageSuggestionsProvider::OfflinePageSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    OfflinePageModel* offline_page_model,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      offline_page_model_(offline_page_model),
      provided_category_(
          category_factory->FromKnownCategory(KnownCategories::OFFLINE_PAGES)),
      pref_service_(pref_service) {
  offline_page_model_->AddObserver(this);
  ReadDismissedIDsFromPrefs();
  FetchOfflinePages();
}

OfflinePageSuggestionsProvider::~OfflinePageSuggestionsProvider() {
  offline_page_model_->RemoveObserver(this);
}

// static
void OfflinePageSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedOfflinePageSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

std::vector<Category> OfflinePageSuggestionsProvider::GetProvidedCategories() {
  return std::vector<Category>({provided_category_});
}

CategoryStatus OfflinePageSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  return category_status_;
}

CategoryInfo OfflinePageSuggestionsProvider::GetCategoryInfo(
    Category category) {
  // TODO(pke): Use the proper string once it's agreed on.
  return CategoryInfo(base::ASCIIToUTF16("Offline pages"),
                      ContentSuggestionsCardLayout::MINIMAL_CARD);
}

void OfflinePageSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  std::string offline_page_id = GetWithinCategoryIDFromUniqueID(suggestion_id);
  dismissed_ids_.insert(offline_page_id);
  StoreDismissedIDsToPrefs();
}

void OfflinePageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(pke): Fetch proper thumbnail from OfflinePageModel once it's available
  // there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, suggestion_id, gfx::Image()));
}

void OfflinePageSuggestionsProvider::ClearCachedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  // Ignored.
}

std::vector<ContentSuggestion>
OfflinePageSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(pke): Make GetDismissedSuggestionsForDebugging asynchronous so this
  // can return proper values.
  DCHECK_EQ(category, provided_category_);
  std::vector<ContentSuggestion> suggestions;
  for (const std::string& dismissed_id : dismissed_ids_) {
    ContentSuggestion suggestion(
        MakeUniqueID(provided_category_, dismissed_id),
        GURL("http://dismissed-offline-page-" + dismissed_id));
    suggestion.set_title(base::UTF8ToUTF16("Title not available"));
    suggestions.push_back(std::move(suggestion));
  }
  return suggestions;
}

void OfflinePageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(category, provided_category_);
  dismissed_ids_.clear();
  StoreDismissedIDsToPrefs();
  FetchOfflinePages();
}

void OfflinePageSuggestionsProvider::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
}

void OfflinePageSuggestionsProvider::OfflinePageModelChanged(
    OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  FetchOfflinePages();
}

void OfflinePageSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  // TODO(pke): Implement, suggestion has to be removed from UI immediately.
}

void OfflinePageSuggestionsProvider::FetchOfflinePages() {
  offline_page_model_->GetAllPages(
      base::Bind(&OfflinePageSuggestionsProvider::OnOfflinePagesLoaded,
                 base::Unretained(this)));
}

void OfflinePageSuggestionsProvider::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& result) {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : result) {
    if (dismissed_ids_.count(base::IntToString(item.offline_id)))
      continue;
    suggestions.push_back(ConvertOfflinePage(item));
    if (suggestions.size() == kMaxSuggestionsCount)
      break;
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

void OfflinePageSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status)
    return;
  category_status_ = new_status;

  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

ContentSuggestion OfflinePageSuggestionsProvider::ConvertOfflinePage(
    const OfflinePageItem& offline_page) const {
  // TODO(pke): Make sure the URL is actually opened as an offline URL.
  // Currently, the browser opens the offline URL and then immediately
  // redirects to the online URL if the device is online.
  ContentSuggestion suggestion(
      MakeUniqueID(provided_category_,
                   base::IntToString(offline_page.offline_id)),
      offline_page.GetOfflineURL());

  // TODO(pke): Sort by most recently visited and only keep the top one of
  // multiple entries for the same URL.
  // TODO(pke): Get more reasonable data from the OfflinePageModel here.
  suggestion.set_title(base::UTF8ToUTF16(offline_page.url.spec()));
  suggestion.set_snippet_text(base::string16());
  suggestion.set_publish_date(offline_page.creation_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(offline_page.url.host()));
  return suggestion;
}

void OfflinePageSuggestionsProvider::ReadDismissedIDsFromPrefs() {
  dismissed_ids_.clear();
  const base::ListValue* list =
      pref_service_->GetList(prefs::kDismissedOfflinePageSuggestions);
  for (const std::unique_ptr<base::Value>& value : *list) {
    std::string dismissed_id;
    bool success = value->GetAsString(&dismissed_id);
    DCHECK(success) << "Failed to parse dismissed offline page ID from prefs";
    dismissed_ids_.insert(std::move(dismissed_id));
  }
}

void OfflinePageSuggestionsProvider::StoreDismissedIDsToPrefs() {
  base::ListValue list;
  for (const std::string& dismissed_id : dismissed_ids_)
    list.AppendString(dismissed_id);
  pref_service_->Set(prefs::kDismissedOfflinePageSuggestions, list);
}

}  // namespace ntp_snippets
