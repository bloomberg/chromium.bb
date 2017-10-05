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
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/ntp/android_content_suggestions_notifier.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/ntp_snippets/dependent_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/switches.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using ntp_snippets::AreAssetDownloadsEnabled;
using ntp_snippets::AreOfflinePageDownloadsEnabled;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::IsBookmarkProviderEnabled;
using ntp_snippets::IsPhysicalWebPageProviderEnabled;
using ntp_snippets::IsRecentTabProviderEnabled;
using ntp_snippets::KnownCategories;
using ntp_snippets::RemoteSuggestion;
using ntp_snippets::RemoteSuggestionsProvider;
using ntp_snippets::UserClassifier;

namespace {

std::unique_ptr<base::DictionaryValue> PrepareSuggestion(
    const ContentSuggestion& suggestion,
    int index) {
  auto entry = base::MakeUnique<base::DictionaryValue>();
  entry->SetString("idWithinCategory", suggestion.id().id_within_category());
  entry->SetString("url", suggestion.url().spec());
  entry->SetString("urlWithFavicon", suggestion.url_with_favicon().spec());
  entry->SetString("title", suggestion.title());
  entry->SetString("snippetText", suggestion.snippet_text());
  entry->SetString("publishDate",
                   TimeFormatShortDateAndTime(suggestion.publish_date()));
  entry->SetString("fetchDate",
                   TimeFormatShortDateAndTime(suggestion.fetch_date()));
  entry->SetString("publisherName", suggestion.publisher_name());
  entry->SetString("id", "content-suggestion-" + base::IntToString(index));
  entry->SetDouble("score", suggestion.score());

  if (suggestion.download_suggestion_extra()) {
    const auto& extra = *suggestion.download_suggestion_extra();
    auto value = base::MakeUnique<base::DictionaryValue>();
    value->SetString("downloadGUID", extra.download_guid);
    value->SetString("targetFilePath",
                     extra.target_file_path.LossyDisplayName());
    value->SetString("mimeType", extra.mime_type);
    value->SetString(
        "offlinePageID",
        base::StringPrintf("0x%016llx", static_cast<long long unsigned int>(
                                            extra.offline_page_id)));
    value->SetBoolean("isDownloadAsset", extra.is_download_asset);
    entry->Set("downloadSuggestionExtra", std::move(value));
  }

  if (suggestion.recent_tab_suggestion_extra()) {
    const auto& extra = *suggestion.recent_tab_suggestion_extra();
    auto value = base::MakeUnique<base::DictionaryValue>();
    value->SetInteger("tabID", extra.tab_id);
    value->SetString(
        "offlinePageID",
        base::StringPrintf("0x%016llx", static_cast<long long unsigned int>(
                                            extra.offline_page_id)));
    entry->Set("recentTabSuggestionExtra", std::move(value));
  }

  if (suggestion.notification_extra()) {
    const auto& extra = *suggestion.notification_extra();
    auto value = base::MakeUnique<base::DictionaryValue>();
    value->SetString("deadline", TimeFormatShortDateAndTime(extra.deadline));
    entry->Set("notificationExtra", std::move(value));
  }

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
    case CategoryStatus::LOADING_ERROR:
      return "LOADING_ERROR";
  }
  return std::string();
}

std::set<variations::VariationID> SnippetsExperiments() {
  std::set<variations::VariationID> result;
  for (const base::Feature* const* feature = ntp_snippets::kAllFeatures;
       *feature; ++feature) {
    base::FieldTrial* trial = base::FeatureList::GetFieldTrial(**feature);
    if (!trial) {
      continue;
    }
    if (trial->GetGroupNameWithoutActivation().empty()) {
      continue;
    }
    for (variations::IDCollectionKey key :
         {variations::GOOGLE_WEB_PROPERTIES,
          variations::GOOGLE_WEB_PROPERTIES_SIGNED_IN,
          variations::GOOGLE_WEB_PROPERTIES_TRIGGER}) {
      const variations::VariationID id = variations::GetGoogleVariationID(
          key, trial->trial_name(), trial->group_name());
      if (id != variations::EMPTY_ID) {
        result.insert(id);
      }
    }
  }
  return result;
}

std::string TimeToJSONTimeString(const base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
}

ntp_snippets::BreakingNewsListener* GetBreakingNewsListener(
    ntp_snippets::ContentSuggestionsService* service) {
  DCHECK(service);
  RemoteSuggestionsProvider* provider =
      service->remote_suggestions_provider_for_debugging();
  DCHECK(provider);
  return static_cast<ntp_snippets::RemoteSuggestionsProviderImpl*>(provider)
      ->breaking_news_listener_for_debugging();
}

}  // namespace

SnippetsInternalsMessageHandler::SnippetsInternalsMessageHandler(
    ntp_snippets::ContentSuggestionsService* content_suggestions_service,
    ntp_snippets::ContextualContentSuggestionsService*
        contextual_content_suggestions_service,
    PrefService* pref_service)
    : content_suggestions_service_observer_(this),
      dom_loaded_(false),
      content_suggestions_service_(content_suggestions_service),
      contextual_content_suggestions_service_(
          contextual_content_suggestions_service),
      remote_suggestions_provider_(
          content_suggestions_service_
              ->remote_suggestions_provider_for_debugging()),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {}

SnippetsInternalsMessageHandler::~SnippetsInternalsMessageHandler() {}

void SnippetsInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearCachedSuggestions",
      base::Bind(&SnippetsInternalsMessageHandler::HandleClearCachedSuggestions,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearClassification",
      base::Bind(&SnippetsInternalsMessageHandler::HandleClearClassification,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearDismissedSuggestions",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleClearDismissedSuggestions,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "download", base::Bind(&SnippetsInternalsMessageHandler::HandleDownload,
                             base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "fetchRemoteSuggestionsInTheBackgroundIn2Seconds",
      base::Bind(&SnippetsInternalsMessageHandler::
                     HandleFetchRemoteSuggestionsInTheBackgroundIn2Seconds,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "fetchContextualSuggestions",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleFetchContextualSuggestions,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "resetNotificationsState",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleResetNotificationsState,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "pushDummySuggestionIn10Seconds",
      base::Bind(&SnippetsInternalsMessageHandler::
                     HandlePushDummySuggestionIn10Seconds,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "refreshContent",
      base::Bind(&SnippetsInternalsMessageHandler::HandleRefreshContent,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleDismissedSuggestions",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleToggleDismissedSuggestions,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "initializationCompleted",
      base::Bind(
          &SnippetsInternalsMessageHandler::HandleInitializationCompleted,
          base::Unretained(this)));

  content_suggestions_service_observer_.Add(content_suggestions_service_);
}

void SnippetsInternalsMessageHandler::OnNewSuggestions(Category category) {
  if (!dom_loaded_) {
    return;
  }
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnCategoryStatusChanged(
    Category category,
    CategoryStatus new_status) {
  if (!dom_loaded_) {
    return;
  }
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnSuggestionInvalidated(
    const ntp_snippets::ContentSuggestion::ID& suggestion_id) {
  if (!dom_loaded_) {
    return;
  }
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::OnFullRefreshRequired() {
  if (!dom_loaded_) {
    return;
  }
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::ContentSuggestionsServiceShutdown() {}

void SnippetsInternalsMessageHandler::HandleInitializationCompleted(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  AllowJavascript();
}

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
  DCHECK_EQ(0u, args->GetSize());

  SendString("remote-status", std::string());

  if (!remote_suggestions_provider_) {
    return;
  }

  remote_suggestions_provider_->ReloadSuggestions();
}

void SnippetsInternalsMessageHandler::HandleClearCachedSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  int category_id;
  if (!args->GetInteger(0, &category_id)) {
    return;
  }

  content_suggestions_service_->ClearCachedSuggestions(
      Category::FromIDValue(category_id));
  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::HandleClearDismissedSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  int category_id;
  if (!args->GetInteger(0, &category_id)) {
    return;
  }

  Category category = Category::FromIDValue(category_id);
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
  if (!args->GetInteger(0, &category_id)) {
    return;
  }
  bool dismissed_visible;
  if (!args->GetBoolean(1, &dismissed_visible)) {
    return;
  }

  Category category = Category::FromIDValue(category_id);
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

void SnippetsInternalsMessageHandler::HandleClearClassification(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  content_suggestions_service_->user_classifier()
      ->ClearClassificationForDebugging();
  SendClassification();
}

void SnippetsInternalsMessageHandler::
    HandleFetchRemoteSuggestionsInTheBackgroundIn2Seconds(
        const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  suggestion_push_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(2),
      base::Bind(&SnippetsInternalsMessageHandler::
                     FetchRemoteSuggestionsInTheBackground,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SnippetsInternalsMessageHandler::HandleFetchContextualSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());
  std::string url_str;
  args->GetString(0, &url_str);
  contextual_content_suggestions_service_->FetchContextualSuggestions(
      GURL(url_str),
      base::BindOnce(
          &SnippetsInternalsMessageHandler::OnContextualSuggestionsFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

void SnippetsInternalsMessageHandler::HandleResetNotificationsState(
    const base::ListValue* args) {
  pref_service_->SetInteger(
      prefs::kContentSuggestionsConsecutiveIgnoredPrefName, 0);
  pref_service_->SetInteger(prefs::kContentSuggestionsNotificationsSentCount,
                            0);
  pref_service_->SetInteger(prefs::kContentSuggestionsNotificationsSentDay, 0);
  AndroidContentSuggestionsNotifier().HideAllNotifications(
      ContentSuggestionsNotificationAction::CONTENT_SUGGESTIONS_HIDE_FRONTMOST);
}

void SnippetsInternalsMessageHandler::OnContextualSuggestionsFetched(
    ntp_snippets::Status status,
    const GURL& url,
    std::vector<ntp_snippets::ContentSuggestion> suggestions) {
  // Ids start in a range distinct from those created by SendContentSuggestions.
  int id = 10000;
  auto suggestions_list = base::MakeUnique<base::ListValue>();
  for (const ContentSuggestion& suggestion : suggestions) {
    suggestions_list->Append(PrepareSuggestion(suggestion, id++));
  }
  base::DictionaryValue result;
  result.Set("list", std::move(suggestions_list));
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveContextualSuggestions", result,
      base::Value(static_cast<int>(status.code)));
}

void SnippetsInternalsMessageHandler::HandlePushDummySuggestionIn10Seconds(
    const base::ListValue* args) {
  suggestion_push_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(10),
      base::Bind(&SnippetsInternalsMessageHandler::PushDummySuggestion,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SnippetsInternalsMessageHandler::SendAllContent() {
  if (!IsJavascriptAllowed()) {
    return;
  }

  SendBoolean(
      "flag-article-suggestions",
      base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature));

  SendBoolean("flag-recent-offline-tab-suggestions",
              IsRecentTabProviderEnabled());
  SendBoolean("flag-offlining-recent-pages-feature",
              base::FeatureList::IsEnabled(
                  offline_pages::kOffliningRecentPagesFeature));

  SendBoolean("flag-asset-download-suggestions", AreAssetDownloadsEnabled());
  SendBoolean("flag-offline-page-download-suggestions",
              AreOfflinePageDownloadsEnabled());

  SendBoolean("flag-bookmark-suggestions", IsBookmarkProviderEnabled());

  SendBoolean("flag-physical-web-page-suggestions",
              IsPhysicalWebPageProviderEnabled());

  SendBoolean("flag-physical-web", base::FeatureList::IsEnabled(
                                       chrome::android::kPhysicalWebFeature));

  SendClassification();
  SendRankerDebugData();
  SendLastRemoteSuggestionsBackgroundFetchTime();
  SendWhetherSuggestionPushingPossible();

  if (remote_suggestions_provider_) {
    const ntp_snippets::RemoteSuggestionsFetcher* fetcher =
        remote_suggestions_provider_->suggestions_fetcher_for_debugging();
    SendString("switch-fetch-url", fetcher->GetFetchUrlForDebugging().spec());
    web_ui()->CallJavascriptFunctionUnsafe(
        "chrome.SnippetsInternals.receiveJson",
        base::Value(fetcher->GetLastJsonForDebugging()));
  }

  CallJavascriptFunction(
      "chrome.SnippetsInternals.receiveDebugLog",
      base::Value(content_suggestions_service_->GetDebugLog()));

  std::set<variations::VariationID> ids = SnippetsExperiments();
  std::vector<std::string> string_ids;
  for (auto id : ids) {
    string_ids.push_back(base::IntToString(id));
  }
  SendString("experiment-ids", base::JoinString(string_ids, ", "));

  SendContentSuggestions();
}

void SnippetsInternalsMessageHandler::SendClassification() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveClassification",
      base::Value(content_suggestions_service_->user_classifier()
                      ->GetUserClassDescriptionForDebugging()),
      base::Value(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::NTP_OPENED)),
      base::Value(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::SUGGESTIONS_SHOWN)),
      base::Value(
          content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
              UserClassifier::Metric::SUGGESTIONS_USED)));
}

void SnippetsInternalsMessageHandler::SendRankerDebugData() {
  std::vector<ntp_snippets::CategoryRanker::DebugDataItem> data =
      content_suggestions_service_->category_ranker()->GetDebugData();

  std::unique_ptr<base::ListValue> items_list(new base::ListValue);
  for (const auto& item : data) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString("label", item.label);
    entry->SetString("content", item.content);
    items_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("list", std::move(items_list));
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveRankerDebugData", result);
}

void SnippetsInternalsMessageHandler::
    SendLastRemoteSuggestionsBackgroundFetchTime() {
  base::Time time = base::Time::FromInternalValue(pref_service_->GetInt64(
      ntp_snippets::prefs::kLastSuccessfulBackgroundFetchTime));
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals."
      "receiveLastRemoteSuggestionsBackgroundFetchTime",
      base::Value(base::TimeFormatShortDateAndTime(time)));
}

void SnippetsInternalsMessageHandler::SendWhetherSuggestionPushingPossible() {
  ntp_snippets::BreakingNewsListener* listener =
      GetBreakingNewsListener(content_suggestions_service_);
  CallJavascriptFunction(
      "chrome.SnippetsInternals."
      "receiveWhetherSuggestionPushingPossible",
      base::Value(listener != nullptr && listener->IsListening()));
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

  if (remote_suggestions_provider_) {
    const std::string& status =
        remote_suggestions_provider_->suggestions_fetcher_for_debugging()
            ->GetLastStatusForDebugging();
    if (!status.empty()) {
      SendString("remote-status", "Finished: " + status);
    }
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
  base::Value string_name(name);
  base::Value string_value(value);

  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.SnippetsInternals.receiveProperty", string_name, string_value);
}

void SnippetsInternalsMessageHandler::FetchRemoteSuggestionsInTheBackground() {
  remote_suggestions_provider_->RefetchInTheBackground(
      RemoteSuggestionsProvider::FetchStatusCallback());
}

void SnippetsInternalsMessageHandler::PushDummySuggestion() {
  std::string json = R"(
      {"categories" : [{
        "id": 1,
        "localizedTitle": "section title",
        "suggestions" : [{
          "ids" : ["http://url.com"],
          "title" : "Pushed Dummy Title %s",
          "snippet" : "Pushed Dummy Snippet",
          "fullPageUrl" : "http://url.com",
          "creationTime" : "%s",
          "expirationTime" : "%s",
          "attribution" : "Pushed Dummy Publisher",
          "imageUrl" : "https://www.google.com/favicon.ico",
          "notificationInfo": {
            "shouldNotify": true,
            "deadline": "2100-01-01T00:00:01.000Z"
           }
        }]
      }]}
  )";

  const base::Time now = base::Time::Now();
  json = base::StringPrintf(
      json.c_str(), base::UTF16ToUTF8(base::TimeFormatTimeOfDay(now)).c_str(),
      TimeToJSONTimeString(now).c_str(),
      TimeToJSONTimeString(now + base::TimeDelta::FromMinutes(60)).c_str());

  gcm::IncomingMessage message;
  message.data["payload"] = json;

  ntp_snippets::BreakingNewsListener* listener =
      GetBreakingNewsListener(content_suggestions_service_);
  DCHECK(listener);
  DCHECK(listener->IsListening());
  static_cast<ntp_snippets::BreakingNewsGCMAppHandler*>(listener)->OnMessage(
      "com.google.breakingnews.gcm", message);
}

void SnippetsInternalsMessageHandler::OnDismissedSuggestionsLoaded(
    Category category,
    std::vector<ContentSuggestion> dismissed_suggestions) {
  if (dismissed_state_[category] == DismissedState::HIDDEN) {
    return;
  }
  dismissed_suggestions_[category] = std::move(dismissed_suggestions);
  dismissed_state_[category] = DismissedState::VISIBLE;
  SendContentSuggestions();
}
