// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/grit/components_scaled_resources.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace ntp_snippets {

namespace {

const size_t kMaxSuggestionsCount = 10;

std::string GetPageId(const DictionaryValue& page_dictionary) {
  std::string raw_resolved_url;
  if (!page_dictionary.GetString(physical_web::kResolvedUrlKey,
                                 &raw_resolved_url)) {
    LOG(DFATAL) << physical_web::kResolvedUrlKey << " field is missing.";
  }
  return raw_resolved_url;
}

}  // namespace

PhysicalWebPageSuggestionsProvider::PhysicalWebPageSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    physical_web::PhysicalWebDataSource* physical_web_data_source,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES)),
      physical_web_data_source_(physical_web_data_source),
      pref_service_(pref_service) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  physical_web_data_source_->RegisterListener(this);
  // TODO(vitaliii): Rewrite initial fetch once crbug.com/667754 is resolved.
  FetchPhysicalWebPages();
}

PhysicalWebPageSuggestionsProvider::~PhysicalWebPageSuggestionsProvider() {
  physical_web_data_source_->UnregisterListener(this);
}

CategoryStatus PhysicalWebPageSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return category_status_;
}

CategoryInfo PhysicalWebPageSuggestionsProvider::GetCategoryInfo(
    Category category) {
  return CategoryInfo(l10n_util::GetStringUTF16(
                          IDS_NTP_PHYSICAL_WEB_PAGE_SUGGESTIONS_SECTION_HEADER),
                      ContentSuggestionsCardLayout::FULL_CARD,
                      /*has_more_action=*/false,
                      /*has_reload_action=*/false,
                      /*has_view_all_action=*/false,
                      /*show_if_empty=*/false,
                      l10n_util::GetStringUTF16(
                          IDS_NTP_PHYSICAL_WEB_PAGE_SUGGESTIONS_SECTION_EMPTY));
}

void PhysicalWebPageSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  dismissed_ids.insert(suggestion_id.id_within_category());
  StoreDismissedIDsToPrefs(dismissed_ids);
}

void PhysicalWebPageSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  base::StringPiece raw_data = resource_bundle.GetRawDataResourceForScale(
      IDR_PHYSICAL_WEB_LOGO_WITH_PADDING, resource_bundle.GetMaxScaleFactor());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 gfx::Image::CreateFrom1xPNGBytes(
                     reinterpret_cast<const unsigned char*>(raw_data.data()),
                     raw_data.size())));
}

void PhysicalWebPageSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  DCHECK_EQ(category, provided_category_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, Status::Success(),
                 base::Passed(GetMostRecentPhysicalWebPagesWithFilter(
                     kMaxSuggestionsCount, known_suggestion_ids))));
}

void PhysicalWebPageSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  ClearDismissedSuggestionsForDebugging(provided_category_);
}

void PhysicalWebPageSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  // Ignored
}

void PhysicalWebPageSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(provided_category_, category);
  std::unique_ptr<ListValue> page_values =
      physical_web_data_source_->GetMetadata();
  const std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const std::unique_ptr<Value>& page_value : *page_values) {
    const DictionaryValue* page_dictionary;
    if (!page_value->GetAsDictionary(&page_dictionary)) {
      LOG(DFATAL) << "Physical Web page is not a dictionary.";
      continue;
    }

    if (dismissed_ids.count(GetPageId(*page_dictionary))) {
      suggestions.push_back(ConvertPhysicalWebPage(*page_dictionary));
    }
  }

  callback.Run(std::move(suggestions));
}

void PhysicalWebPageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreDismissedIDsToPrefs(std::set<std::string>());
  FetchPhysicalWebPages();
}

// static
void PhysicalWebPageSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedPhysicalWebPageSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void PhysicalWebPageSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

void PhysicalWebPageSuggestionsProvider::FetchPhysicalWebPages() {
  DCHECK_EQ(CategoryStatus::AVAILABLE, category_status_);
  observer()->OnNewSuggestions(this, provided_category_,
                               GetMostRecentPhysicalWebPagesWithFilter(
                                   kMaxSuggestionsCount,
                                   /*excluded_ids=*/std::set<std::string>()));
}

std::vector<ContentSuggestion>
PhysicalWebPageSuggestionsProvider::GetMostRecentPhysicalWebPagesWithFilter(
    int max_quantity,
    const std::set<std::string>& excluded_ids) {
  std::unique_ptr<ListValue> page_values =
      physical_web_data_source_->GetMetadata();

  // These is to filter out dismissed suggestions and at the same time prune the
  // dismissed IDs list removing nonavailable pages (this is need since some
  // OnLost() calls may have been missed).
  const std::set<std::string> old_dismissed_ids = ReadDismissedIDsFromPrefs();
  std::set<std::string> new_dismissed_ids;
  std::vector<const DictionaryValue*> page_dictionaries;
  for (const std::unique_ptr<Value>& page_value : *page_values) {
    const DictionaryValue* page_dictionary;
    if (!page_value->GetAsDictionary(&page_dictionary)) {
      LOG(DFATAL) << "Physical Web page is not a dictionary.";
      continue;
    }

    const std::string page_id = GetPageId(*page_dictionary);
    if (!excluded_ids.count(page_id) && !old_dismissed_ids.count(page_id)) {
      page_dictionaries.push_back(page_dictionary);
    }

    if (old_dismissed_ids.count(page_id)) {
      new_dismissed_ids.insert(page_id);
    }
  }

  if (old_dismissed_ids.size() != new_dismissed_ids.size()) {
    StoreDismissedIDsToPrefs(new_dismissed_ids);
  }

  std::sort(page_dictionaries.begin(), page_dictionaries.end(),
            [](const DictionaryValue* left, const DictionaryValue* right) {
              double left_distance, right_distance;
              bool success = left->GetDouble(physical_web::kDistanceEstimateKey,
                                             &left_distance);
              success = right->GetDouble(physical_web::kDistanceEstimateKey,
                                         &right_distance) &&
                        success;
              if (!success) {
                LOG(DFATAL) << "Distance field is missing.";
              }
              return left_distance < right_distance;
            });

  std::vector<ContentSuggestion> suggestions;
  for (const DictionaryValue* page_dictionary : page_dictionaries) {
    if (static_cast<int>(suggestions.size()) == max_quantity) {
      break;
    }
    suggestions.push_back(ConvertPhysicalWebPage(*page_dictionary));
  }

  return suggestions;
}

ContentSuggestion PhysicalWebPageSuggestionsProvider::ConvertPhysicalWebPage(
    const DictionaryValue& page) const {
  std::string scanned_url, raw_resolved_url, title, description;
  bool success = page.GetString(physical_web::kScannedUrlKey, &scanned_url);
  success = page.GetString(physical_web::kResolvedUrlKey, &raw_resolved_url) &&
            success;
  success = page.GetString(physical_web::kTitleKey, &title) && success;
  success =
      page.GetString(physical_web::kDescriptionKey, &description) && success;
  if (!success) {
    LOG(DFATAL) << "Expected field is missing.";
  }

  const GURL resolved_url(raw_resolved_url);
  ContentSuggestion suggestion(provided_category_, GetPageId(page),
                               resolved_url);
  DCHECK(base::IsStringUTF8(title));
  suggestion.set_title(base::UTF8ToUTF16(title));
  suggestion.set_publisher_name(base::UTF8ToUTF16(resolved_url.host()));
  DCHECK(base::IsStringUTF8(description));
  suggestion.set_snippet_text(base::UTF8ToUTF16(description));
  return suggestion;
}

// PhysicalWebListener implementation.
void PhysicalWebPageSuggestionsProvider::OnFound(const GURL& url) {
  FetchPhysicalWebPages();
}

void PhysicalWebPageSuggestionsProvider::OnLost(const GURL& url) {
  InvalidateSuggestion(url.spec());
}

void PhysicalWebPageSuggestionsProvider::OnDistanceChanged(
    const GURL& url,
    double distance_estimate) {
  FetchPhysicalWebPages();
}

void PhysicalWebPageSuggestionsProvider::InvalidateSuggestion(
    const std::string& page_id) {
  observer()->OnSuggestionInvalidated(
      this, ContentSuggestion::ID(provided_category_, page_id));

  // Remove |page_id| from dismissed suggestions, if present.
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  auto it = dismissed_ids.find(page_id);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(dismissed_ids);
  }
}

std::set<std::string>
PhysicalWebPageSuggestionsProvider::ReadDismissedIDsFromPrefs() const {
  return prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedPhysicalWebPageSuggestions);
}

void PhysicalWebPageSuggestionsProvider::StoreDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  prefs::StoreDismissedIDsToPrefs(pref_service_,
                                  prefs::kDismissedPhysicalWebPageSuggestions,
                                  dismissed_ids);
}

}  // namespace ntp_snippets
