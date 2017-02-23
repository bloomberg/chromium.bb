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
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ntp_snippets {

namespace {

const size_t kMaxSuggestionsCount = 10;

std::string GetPageId(const physical_web::Metadata& page_metadata) {
  return page_metadata.resolved_url.spec();
}

bool CompareByDistance(const physical_web::Metadata& left,
                       const physical_web::Metadata& right) {
  // When there is no estimate, the value is <= 0, so we implicitly treat it as
  // infinity.
  bool is_left_estimated = left.distance_estimate > 0;
  bool is_right_estimated = right.distance_estimate > 0;

  if (is_left_estimated != is_right_estimated)
    return is_left_estimated;
  return left.distance_estimate < right.distance_estimate;
}

void FilterOutByGroupId(
    physical_web::MetadataList& page_metadata_list) {
  // |std::unique| only removes duplicates that immediately follow each other.
  // Thus, first, we have to sort by group_id and distance and only then remove
  // duplicates.
  std::sort(page_metadata_list.begin(), page_metadata_list.end(),
            [](const physical_web::Metadata& left,
               const physical_web::Metadata& right) {
              if (left.group_id != right.group_id) {
                return left.group_id < right.group_id;
              }

              // We want closest pages first, so in case of same group_id we
              // sort by distance.
              return CompareByDistance(left, right);
            });

  // Each empty group_id must be treated as unique, so we do not apply
  // std::unique to them at all.
  auto nonempty_group_id_begin = std::find_if(
      page_metadata_list.begin(), page_metadata_list.end(),
      [](const physical_web::Metadata& page) {
        return !page.group_id.empty();
      });

  auto new_end = std::unique(
      nonempty_group_id_begin, page_metadata_list.end(),
      [](const physical_web::Metadata& left,
         const physical_web::Metadata& right) {
        return left.group_id == right.group_id;
      });

  page_metadata_list.erase(new_end, page_metadata_list.end());
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
                      /*has_fetch_action=*/false,
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
  std::vector<ContentSuggestion> suggestions =
      GetMostRecentPhysicalWebPagesWithFilter(kMaxSuggestionsCount,
                                              known_suggestion_ids);
  AppendToShownScannedUrls(suggestions);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, Status::Success(),
                            base::Passed(std::move(suggestions))));
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
  std::unique_ptr<physical_web::MetadataList> page_metadata_list =
      physical_web_data_source_->GetMetadataList();
  const std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const auto& page_metadata : *page_metadata_list) {
    if (dismissed_ids.count(GetPageId(page_metadata))) {
      suggestions.push_back(ConvertPhysicalWebPage(page_metadata));
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
  std::vector<ContentSuggestion> suggestions =
      GetMostRecentPhysicalWebPagesWithFilter(
          kMaxSuggestionsCount,
          /*excluded_ids=*/std::set<std::string>());
  shown_resolved_urls_by_scanned_url_.clear();
  AppendToShownScannedUrls(suggestions);
  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

std::vector<ContentSuggestion>
PhysicalWebPageSuggestionsProvider::GetMostRecentPhysicalWebPagesWithFilter(
    int max_count,
    const std::set<std::string>& excluded_ids) {
  std::unique_ptr<physical_web::MetadataList> page_metadata_list =
      physical_web_data_source_->GetMetadataList();

  // These is to filter out dismissed suggestions and at the same time prune the
  // dismissed IDs list removing nonavailable pages (this is needed since some
  // OnLost() calls may have been missed).
  const std::set<std::string> old_dismissed_ids = ReadDismissedIDsFromPrefs();
  std::set<std::string> new_dismissed_ids;
  physical_web::MetadataList filtered_metadata_list;
  for (const auto& page_metadata : *page_metadata_list) {
    const std::string page_id = GetPageId(page_metadata);
    if (!excluded_ids.count(page_id) && !old_dismissed_ids.count(page_id)) {
      filtered_metadata_list.push_back(page_metadata);
    }

    if (old_dismissed_ids.count(page_id)) {
      new_dismissed_ids.insert(page_id);
    }
  }

  if (old_dismissed_ids.size() != new_dismissed_ids.size()) {
    StoreDismissedIDsToPrefs(new_dismissed_ids);
  }

  FilterOutByGroupId(filtered_metadata_list);

  std::sort(filtered_metadata_list.begin(), filtered_metadata_list.end(),
            CompareByDistance);

  std::vector<ContentSuggestion> suggestions;
  for (const auto& page_metadata : filtered_metadata_list) {
    if (static_cast<int>(suggestions.size()) == max_count) {
      break;
    }
    suggestions.push_back(ConvertPhysicalWebPage(page_metadata));
  }

  return suggestions;
}

ContentSuggestion PhysicalWebPageSuggestionsProvider::ConvertPhysicalWebPage(
    const physical_web::Metadata& page) const {
  ContentSuggestion suggestion(provided_category_, GetPageId(page),
                               page.resolved_url);
  DCHECK(base::IsStringUTF8(page.title));
  suggestion.set_title(base::UTF8ToUTF16(page.title));
  suggestion.set_publisher_name(base::UTF8ToUTF16(page.resolved_url.host()));
  DCHECK(base::IsStringUTF8(page.description));
  suggestion.set_snippet_text(base::UTF8ToUTF16(page.description));
  return suggestion;
}

// PhysicalWebListener implementation.
void PhysicalWebPageSuggestionsProvider::OnFound(const GURL& url) {
  FetchPhysicalWebPages();
}

void PhysicalWebPageSuggestionsProvider::OnLost(const GURL& url) {
  auto it = shown_resolved_urls_by_scanned_url_.find(url);
  if (it == shown_resolved_urls_by_scanned_url_.end()) {
    // The notification is propagated further in case the suggestion is shown on
    // old NTPs (created before last |shown_resolved_urls_by_scanned_url_|
    // update).

    // TODO(vitaliii): Use |resolved_url| here when it is available. Currently
    // there is no way to find out |resolved_url|, which corresponds to this
    // |scanned_url| (the metadata has been already removed from the Physical
    // Web list). We use |scanned_url| (it may be the same as |resolved_url|,
    // otherwise nothing happens), however, we should use the latter once it is
    // provided (e.g. as an argument).
    InvalidateSuggestion(url.spec());
    return;
  }

  // This is not a reference, because the multimap pair will be removed below.
  const GURL lost_resolved_url = it->second;
  shown_resolved_urls_by_scanned_url_.erase(it);
  if (std::find_if(shown_resolved_urls_by_scanned_url_.begin(),
                   shown_resolved_urls_by_scanned_url_.end(),
                   [lost_resolved_url](const std::pair<GURL, GURL>& pair) {
                     return lost_resolved_url == pair.second;
                   }) == shown_resolved_urls_by_scanned_url_.end()) {
    // There are no more beacons for this URL.
    InvalidateSuggestion(lost_resolved_url.spec());
  }
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

void PhysicalWebPageSuggestionsProvider::AppendToShownScannedUrls(
    const std::vector<ContentSuggestion>& suggestions) {
  std::unique_ptr<physical_web::MetadataList> page_metadata_list =
      physical_web_data_source_->GetMetadataList();
  for (const auto& page_metadata : *page_metadata_list) {
    if (std::find_if(suggestions.begin(), suggestions.end(),
                     [page_metadata](const ContentSuggestion& suggestion) {
                       return suggestion.url() == page_metadata.resolved_url;
                     }) != suggestions.end()) {
      shown_resolved_urls_by_scanned_url_.insert(std::make_pair(
          page_metadata.scanned_url, page_metadata.resolved_url));
    }
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
