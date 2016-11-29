// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace ntp_snippets {

namespace {

const size_t kMaxSuggestionsCount = 10;

}  // namespace

PhysicalWebPageSuggestionsProvider::PhysicalWebPageSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory,
    physical_web::PhysicalWebDataSource* physical_web_data_source)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE),
      provided_category_(category_factory->FromKnownCategory(
          KnownCategories::PHYSICAL_WEB_PAGES)),
      physical_web_data_source_(physical_web_data_source) {
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
  // TODO(vitaliii): Use the proper string once it has been agreed on.
  // TODO(vitaliii): Use a translateable string. (crbug.com/667764)
  // TODO(vitaliii): Implement More action. (crbug.com/667759)
  return CategoryInfo(
      base::ASCIIToUTF16("Physical web pages"),
      ContentSuggestionsCardLayout::FULL_CARD,
      /*has_more_action=*/false,
      /*has_reload_action=*/false,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_SUGGESTIONS_SECTION_EMPTY));
}

void PhysicalWebPageSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  // TODO(vitaliii): Implement this and then
  // ClearDismissedSuggestionsForDebugging. (crbug.com/667766)
}

void PhysicalWebPageSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(vitaliii): Implement. (crbug.com/667765)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void PhysicalWebPageSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  LOG(DFATAL)
      << "PhysicalWebPageSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, Status(StatusCode::PERMANENT_ERROR,
                                  "PhysicalWebPageSuggestionsProvider "
                                  "has no |Fetch| functionality!"),
                 base::Passed(std::vector<ContentSuggestion>())));
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
  // Not implemented.
  callback.Run(std::vector<ContentSuggestion>());
}

void PhysicalWebPageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(vitaliii): Implement when dismissed suggestions are supported.
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
  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  std::unique_ptr<ListValue> page_values =
      physical_web_data_source_->GetMetadata();

  std::vector<const DictionaryValue*> page_dictionaries;
  for (const std::unique_ptr<Value>& page_value : *page_values) {
    const DictionaryValue* page_dictionary;
    if (!page_value->GetAsDictionary(&page_dictionary)) {
      LOG(DFATAL) << "Physical Web page is not a dictionary.";
    }
    page_dictionaries.push_back(page_dictionary);
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
    suggestions.push_back(ConvertPhysicalWebPage(*page_dictionary));
    if (suggestions.size() == kMaxSuggestionsCount) {
      break;
    }
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

ContentSuggestion PhysicalWebPageSuggestionsProvider::ConvertPhysicalWebPage(
    const DictionaryValue& page) const {
  std::string scanned_url, raw_resolved_url, title, description;
  int scan_timestamp;
  bool success = page.GetString(physical_web::kScannedUrlKey, &scanned_url);
  success = page.GetInteger(physical_web::kScanTimestampKey, &scan_timestamp) &&
            success;
  success = page.GetString(physical_web::kResolvedUrlKey, &raw_resolved_url) &&
            success;
  success = page.GetString(physical_web::kTitleKey, &title) && success;
  success =
      page.GetString(physical_web::kDescriptionKey, &description) && success;
  if (!success) {
    LOG(DFATAL) << "Expected field is missing.";
  }

  const GURL resolved_url(raw_resolved_url);
  ContentSuggestion suggestion(provided_category_, resolved_url.spec(),
                               resolved_url);
  DCHECK(base::IsStringUTF8(title));
  suggestion.set_title(base::UTF8ToUTF16(title));
  // TODO(vitaliii): Set the time properly once the proper value is provided
  // (see crbug.com/667722).
  suggestion.set_publish_date(
      base::Time::FromTimeT(static_cast<time_t>(scan_timestamp)));
  suggestion.set_publisher_name(base::UTF8ToUTF16(resolved_url.host()));
  DCHECK(base::IsStringUTF8(description));
  suggestion.set_snippet_text(base::UTF8ToUTF16(description));
  return suggestion;
}

// PhysicalWebListener implementation.
void PhysicalWebPageSuggestionsProvider::OnFound(const std::string& url) {
  FetchPhysicalWebPages();
}

void PhysicalWebPageSuggestionsProvider::OnLost(const std::string& url) {
  // TODO(vitaliii): Do not refetch, but just update the current state.
  FetchPhysicalWebPages();
}

void PhysicalWebPageSuggestionsProvider::OnDistanceChanged(
    const std::string& url,
    double distance_estimate) {
  FetchPhysicalWebPages();
}

}  // namespace ntp_snippets
