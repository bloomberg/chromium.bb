// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/snippets_internals_message_handler.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/switches.h"
#include "content/public/browser/web_ui.h"

using ntp_snippets::ContentSuggestion;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;
using ntp_snippets::UserClassifier;

namespace {

std::unique_ptr<base::DictionaryValue> PrepareSuggestion(
    const ContentSuggestion& suggestion,
    int index) {
  auto entry = base::MakeUnique<base::DictionaryValue>();
  entry->SetString("idWithinCategory", suggestion.id().id_within_category());
  entry->SetString("url", suggestion.url().spec());
  entry->SetString("ampUrl", suggestion.amp_url().spec());
  entry->SetString("title", suggestion.title());
  entry->SetString("snippetText", suggestion.snippet_text());
  entry->SetString("publishDate",
                   TimeFormatShortDateAndTime(suggestion.publish_date()));
  entry->SetString("publisherName", suggestion.publisher_name());
  entry->SetString("id", "content-suggestion-" + base::IntToString(index));
  return entry;
}

std::string GetCategoryStatusName(CategoryStatus status) {
  switch (status) {
    case CategoryStatus::INITIALIZING:
      return "INITIALIZING";
    case CategoryStatus::AVAILABLE:
      return "AVAILABLE";
    case CategoryStatus::AVAILABLE_LOADING:
      return "AVAILABLE_LOADING";
    case CategoryStatus::NOT_PROVIDED:
      return "NOT_PROVIDED";
    case CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
      return "ALL_SUGGESTIONS_EXPLICITLY_DISABLED";
    case CategoryStatus::CATEGORY_EXPLICITLY_DISABLED:
      return "CATEGORY_EXPLICITLY_DISABLED";
    case CategoryStatus::SIGNED_OUT:
      return "SIGNED_OUT";
    case CategoryStatus::LOADING_ERROR:
      return "LOADING_ERROR";
  }
  return std::string();
}

}  // namespace

SnippetsInternalsMessageHandler::SnippetsInternalsMessageHandler()
    : content_suggestions_service_observer_(this),
      dom_loaded_(false),
      ntp_snippets_service_(nullptr),
      content_suggestions_service_(nullptr),
      weak_ptr_factory_(this) {}

SnippetsInternalsMessageHandler::~SnippetsInternalsMessageHandler() {}

void SnippetsInternalsMessageHandler::RegisterMessages() {
  // Additional initialization (web_ui() does not work from the constructor).
  Profile* profile = Profile::FromWebUI(web_ui());

  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetInstance()->GetForProfile(profile);
  content_suggestions_service_observer_.Add(content_suggestions_service_);

  ntp_snippets_service_ = content_suggestions_service_->ntp_snippets_service();

  web_ui()->RegisterMessageCallback(
      "refreshContent",
      base::Bind(&SnippetsInternalsMessageHandler::HandleRefreshContent,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "download", base::Bind(&SnippetsInternalsMessageHandler::HandleDownload,
                             base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearCachedSuggestions",
      base::Bind(&SnippetsInternalsMessageHandler::HandleClearCachedSuggestions,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearDismissedSuggestions",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleClearDismissedSuggestions,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleDismissedSuggestions",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleToggleDismissedSuggestions,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearClassification",
      base::Bind(
          &SnippetsInternalsMessageHandler::ClearClassification,
          base::Unretained(this)));
}

void SnippetsInternalsMessageHandler::OnNewSuggestions(Category category) {
  if (!dom_loaded_)
    return;
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnCategoryStatusChanged(
    Category category,
    CategoryStatus new_status) {
  if (!dom_loaded_)
    return;
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnSuggestionInvalidated(
    const ntp_snippets::ContentSuggestion::ID& suggestion_id) {
  if (!dom_loaded_)
    return;
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnFullRefreshRequired() {
  if (!dom_loaded_) {
    return;
  }
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::ContentSuggestionsServiceShutdown() {}

void SnippetsInternalsMessageHandler::HandleRefreshContent(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  dom_loaded_ = true;

  for (std::pair<const Category, DismissedState>& category_state_pair :
       dismissed_state_) {
    if (category_state_pair.second == DismissedState::VISIBLE) {
      category_state_pair.second = DismissedState::LOADING;
      content_suggestions_service_->GetDismissedSuggestionsForDebugging(
          category_state_pair.first,
          base::Bind(
              &SnippetsInternalsMessageHandler::OnDismissedSuggestionsLoaded,
              weak_ptr_factory_.GetWeakPtr(), category_state_pair.first));
    }
  }

  SendAllContent();
}

void SnippetsInternalsMessageHandler::HandleDownload(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  SendString("hosts-status", std::string());

  if (!ntp_snippets_service_)
    return;

  std::string hosts_string;
  if (!args->GetString(0, &hosts_string))
    return;

  std::vector<std::string> hosts_vector = base::SplitString(
      hosts_string, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<std::string> hosts(hosts_vector.begin(), hosts_vector.end());

  ntp_snippets_service_->FetchSnippetsFromHosts(hosts,
                                                /*interactive_request=*/true);
}

void SnippetsInternalsMessageHandler::HandleClearCachedSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  int category_id;
  if (!args->GetInteger(0, &category_id))
    return;

  Category category =
      content_suggestions_service_->category_factory()->FromIDValue(
          category_id);
  content_suggestions_service_->ClearCachedSuggestions(category);
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::HandleClearDismissedSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  int category_id;
  if (!args->GetInteger(0, &category_id))
    return;

  Category category =
      content_suggestions_service_->category_factory()->FromIDValue(
          category_id);
  content_suggestions_service_->ClearDismissedSuggestionsForDebugging(category);
  SendContentSuggestions();
  dismissed_state_[category] = DismissedState::LOADING;
  content_suggestions_service_->GetDismissedSuggestionsForDebugging(
      category,
      base::Bind(&SnippetsInternalsMessageHandler::OnDismissedSuggestionsLoaded,
                 weak_ptr_factory_.GetWeakPtr(), category));
}

void SnippetsInternalsMessageHandler::HandleToggleDismissedSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(2u, args->GetSize());

  int category_id;
  if (!args->GetInteger(0, &category_id))
    return;
  bool dismissed_visible;
  if (!args->GetBoolean(1, &dismissed_visible))
    return;

  Category category =
      content_suggestions_service_->category_factory()->FromIDValue(
          category_id);
  if (dismissed_visible) {
    dismissed_state_[category] = DismissedState::LOADING;
    content_suggestions_service_->GetDismissedSuggestionsForDebugging(
        category,
        base::Bind(
            &SnippetsInternalsMessageHandler::OnDismissedSuggestionsLoaded,
            weak_ptr_factory_.GetWeakPtr(), category));
  } else {
    dismissed_state_[category] = DismissedState::HIDDEN;
    dismissed_suggestions_[category].clear();
  }
}

void SnippetsInternalsMessageHandler::ClearClassification(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  content_suggestions_service_->user_classifier()
      ->ClearClassificationForDebugging();
  SendClassification();
}

void SnippetsInternalsMessageHandler::SendAllContent() {
  SendBoolean("flag-snippets", base::FeatureList::IsEnabled(
                                   ntp_snippets::kContentSuggestionsFeature));
  SendBoolean(
      "flag-article-suggestions",
      base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature));
  SendBoolean("flag-recent-offline-tab-suggestions",
              base::FeatureList::IsEnabled(
                  ntp_snippets::kRecentOfflineTabSuggestionsFeature));
  SendBoolean(
      "flag-asset-download-suggestions",
      base::FeatureList::IsEnabled(features::kAssetDownloadSuggestionsFeature));
  SendBoolean("flag-offline-page-download-suggestions",
              base::FeatureList::IsEnabled(
                  features::kOfflinePageDownloadSuggestionsFeature));
  SendBoolean(
      "flag-bookmark-suggestions",
      base::FeatureList::IsEnabled(ntp_snippets::kBookmarkSuggestionsFeature));

  SendBoolean("flag-physical-web-page-suggestions",
              base::FeatureList::IsEnabled(
                  ntp_snippets::kPhysicalWebPageSuggestionsFeature));

  SendClassification();

  if (ntp_snippets_service_) {
    switch (ntp_snippets_service_->snippets_fetcher()->personalization()) {
      case ntp_snippets::NTPSnippetsFetcher::Personalization::kPersonal:
        SendString("switch-personalized", "Only personalized");
        break;
      case ntp_snippets::NTPSnippetsFetcher::Personalization::kBoth:
        SendString("switch-personalized",
                   "Both personalized and non-personalized");
        break;
      case ntp_snippets::NTPSnippetsFetcher::Personalization::kNonPersonal:
        SendString("switch-personalized", "Only non-personalized");
        break;
    }

    SendString("switch-fetch-url",
               ntp_snippets_service_->snippets_fetcher()->fetch_url().spec());
    web_ui()->CallJavascriptFunctionUnsafe(
        "chrome.SnippetsInternals.receiveJson",
        base::StringValue(
            ntp_snippets_service_->snippets_fetcher()->last_json()));
  }

  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::SendClassification() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveClassification",
      base::StringValue(content_suggestions_service_->user_classifier()
                            ->GetUserClassDescriptionForDebugging()),
      base::FundamentalValue(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::NTP_OPENED)),
      base::FundamentalValue(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::SUGGESTIONS_SHOWN)),
      base::FundamentalValue(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::SUGGESTIONS_USED)));
}

void SnippetsInternalsMessageHandler::SendContentSuggestions() {
  std::unique_ptr<base::ListValue> categories_list(new base::ListValue);

  int index = 0;
  for (Category category : content_suggestions_service_->GetCategories()) {
    CategoryStatus status =
        content_suggestions_service_->GetCategoryStatus(category);
    base::Optional<CategoryInfo> info =
        content_suggestions_service_->GetCategoryInfo(category);
    DCHECK(info);
    const std::vector<ContentSuggestion>& suggestions =
        content_suggestions_service_->GetSuggestionsForCategory(category);

    std::unique_ptr<base::ListValue> suggestions_list(new base::ListValue);
    for (const ContentSuggestion& suggestion : suggestions) {
      suggestions_list->Append(PrepareSuggestion(suggestion, index++));
    }

    std::unique_ptr<base::ListValue> dismissed_list(new base::ListValue);
    for (const ContentSuggestion& suggestion :
         dismissed_suggestions_[category]) {
      dismissed_list->Append(PrepareSuggestion(suggestion, index++));
    }

    std::unique_ptr<base::DictionaryValue> category_entry(
        new base::DictionaryValue);
    category_entry->SetInteger("categoryId", category.id());
    category_entry->SetString(
        "dismissedContainerId",
        "dismissed-suggestions-" + base::IntToString(category.id()));
    category_entry->SetString("title", info->title());
    category_entry->SetString("status", GetCategoryStatusName(status));
    category_entry->Set("suggestions", std::move(suggestions_list));
    category_entry->Set("dismissedSuggestions", std::move(dismissed_list));
    categories_list->Append(std::move(category_entry));
  }

  if (ntp_snippets_service_) {
    const std::string& status =
        ntp_snippets_service_->snippets_fetcher()->last_status();
    if (!status.empty())
      SendString("hosts-status", "Finished: " + status);
  }

  base::DictionaryValue result;
  result.Set("list", std::move(categories_list));
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveContentSuggestions", result);
}

void SnippetsInternalsMessageHandler::SendBoolean(const std::string& name,
                                                  bool value) {
  SendString(name, value ? "True" : "False");
}

void SnippetsInternalsMessageHandler::SendString(const std::string& name,
                                                 const std::string& value) {
  base::StringValue string_name(name);
  base::StringValue string_value(value);

  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveProperty", string_name, string_value);
}

void SnippetsInternalsMessageHandler::OnDismissedSuggestionsLoaded(
    Category category,
    std::vector<ContentSuggestion> dismissed_suggestions) {
  if (dismissed_state_[category] == DismissedState::HIDDEN)
    return;
  dismissed_suggestions_[category] = std::move(dismissed_suggestions);
  dismissed_state_[category] = DismissedState::VISIBLE;
  SendContentSuggestions();
}
