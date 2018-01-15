// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline/offline_internals_ui_message_handler.h"

#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetched_pages_notifier.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "content/public/browser/web_ui.h"
#include "net/base/network_change_notifier.h"

namespace offline_internals {

namespace {

std::string GetStringFromDeletePageResult(
    offline_pages::DeletePageResult value) {
  switch (value) {
    case offline_pages::DeletePageResult::SUCCESS:
      return "Success";
    case offline_pages::DeletePageResult::CANCELLED:
      return "Cancelled";
    case offline_pages::DeletePageResult::STORE_FAILURE:
      return "Store failure";
    case offline_pages::DeletePageResult::DEVICE_FAILURE:
      return "Device failure";
    case offline_pages::DeletePageResult::NOT_FOUND:
      return "Not found";
    case offline_pages::DeletePageResult::RESULT_COUNT:
      break;
  }
  NOTREACHED();
  return "Unknown";
}

std::string GetStringFromDeleteRequestResults(
    const offline_pages::MultipleItemStatuses& results) {
  // If any requests failed, return "failure", else "success".
  for (const auto& result : results) {
    if (result.second == offline_pages::ItemActionStatus::STORE_ERROR)
      return "Store failure, could not delete one or more requests";
  }

  return "Success";
}

std::string GetStringFromSavePageStatus() {
  return "Available";
}

}  // namespace

OfflineInternalsUIMessageHandler::OfflineInternalsUIMessageHandler()
    : offline_page_model_(nullptr),
      request_coordinator_(nullptr),
      prefetch_service_(nullptr),
      weak_ptr_factory_(this) {}

OfflineInternalsUIMessageHandler::~OfflineInternalsUIMessageHandler() {}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::vector<int64_t> offline_ids;
  const base::ListValue* offline_ids_from_arg;
  args->GetList(1, &offline_ids_from_arg);

  for (size_t i = 0; i < offline_ids_from_arg->GetSize(); i++) {
    std::string value;
    offline_ids_from_arg->GetString(i, &value);
    int64_t int_value;
    base::StringToInt64(value, &int_value);
    offline_ids.push_back(int_value);
  }

  offline_page_model_->DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedRequests(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::vector<int64_t> offline_ids;
  const base::ListValue* offline_ids_from_arg = nullptr;
  args->GetList(1, &offline_ids_from_arg);

  for (size_t i = 0; i < offline_ids_from_arg->GetSize(); i++) {
    std::string value;
    offline_ids_from_arg->GetString(i, &value);
    int64_t int_value;
    base::StringToInt64(value, &int_value);
    offline_ids.push_back(int_value);
  }

  // Call RequestCoordinator to delete them
  if (request_coordinator_) {
    request_coordinator_->RemoveRequests(
        offline_ids,
        base::Bind(
            &OfflineInternalsUIMessageHandler::HandleDeletedRequestsCallback,
            weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback(
    std::string callback_id,
    offline_pages::DeletePageResult result) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(GetStringFromDeletePageResult(result)));
}

void OfflineInternalsUIMessageHandler::HandleDeletedRequestsCallback(
    std::string callback_id,
    const offline_pages::MultipleItemStatuses& results) {
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(GetStringFromDeleteRequestResults(results)));
}

void OfflineInternalsUIMessageHandler::HandleStoredPagesCallback(
    std::string callback_id,
    const offline_pages::MultipleOfflinePageItemResult& pages) {
  base::ListValue results;

  for (const auto& page : pages) {
    auto offline_page = std::make_unique<base::DictionaryValue>();
    offline_page->SetString("onlineUrl", page.url.spec());
    offline_page->SetString("namespace", page.client_id.name_space);
    offline_page->SetDouble("size", page.file_size);
    offline_page->SetString("id", std::to_string(page.offline_id));
    offline_page->SetString("filePath", page.file_path.MaybeAsASCII());
    offline_page->SetDouble("creationTime", page.creation_time.ToJsTime());
    offline_page->SetDouble("lastAccessTime", page.last_access_time.ToJsTime());
    offline_page->SetInteger("accessCount", page.access_count);
    offline_page->SetString("originalUrl", page.original_url.spec());
    offline_page->SetString("requestOrigin", page.request_origin);
    results.Append(std::move(offline_page));
  }
  ResolveJavascriptCallback(base::Value(callback_id), results);
}

void OfflineInternalsUIMessageHandler::HandleRequestQueueCallback(
    std::string callback_id,
    offline_pages::GetRequestsResult result,
    std::vector<std::unique_ptr<offline_pages::SavePageRequest>> requests) {
  base::ListValue save_page_requests;
  if (result == offline_pages::GetRequestsResult::SUCCESS) {
    for (const auto& request : requests) {
      auto save_page_request = std::make_unique<base::DictionaryValue>();
      save_page_request->SetString("onlineUrl", request->url().spec());
      save_page_request->SetDouble("creationTime",
                                   request->creation_time().ToJsTime());
      save_page_request->SetString("status", GetStringFromSavePageStatus());
      save_page_request->SetString("namespace",
                                   request->client_id().name_space);
      save_page_request->SetDouble("lastAttemptTime",
                                   request->last_attempt_time().ToJsTime());
      save_page_request->SetString("id", std::to_string(request->request_id()));
      save_page_request->SetString("originalUrl",
                                   request->original_url().spec());
      save_page_request->SetString("requestOrigin", request->request_origin());
      save_page_requests.Append(std::move(save_page_request));
    }
  }
  ResolveJavascriptCallback(base::Value(callback_id), save_page_requests);
}

void OfflineInternalsUIMessageHandler::HandleGetRequestQueue(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (request_coordinator_) {
    request_coordinator_->queue()->GetRequests(base::Bind(
        &OfflineInternalsUIMessageHandler::HandleRequestQueueCallback,
        weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::Value(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleGetStoredPages(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (offline_page_model_) {
    offline_page_model_->GetAllPages(
        base::Bind(&OfflineInternalsUIMessageHandler::HandleStoredPagesCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::Value(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPageModel(
    const base::ListValue* args) {
  AllowJavascript();
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  if (offline_page_model_)
    offline_page_model_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetNetworkStatus(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(
      *callback_id,
      base::Value(net::NetworkChangeNotifier::IsOffline() ? "Offline"
                                                          : "Online"));
}

void OfflineInternalsUIMessageHandler::HandleScheduleNwake(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  if (prefetch_service_) {
    prefetch_service_->GetPrefetchBackgroundTaskHandler()
        ->EnsureTaskScheduled();
    ResolveJavascriptCallback(*callback_id, base::Value("Scheduled."));
  } else {
    RejectJavascriptCallback(*callback_id,
                             base::Value("No prefetch service available."));
  }
}

void OfflineInternalsUIMessageHandler::HandleCancelNwake(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  if (prefetch_service_) {
    prefetch_service_->GetPrefetchBackgroundTaskHandler()
        ->CancelBackgroundTask();
    ResolveJavascriptCallback(*callback_id, base::Value("Cancelled."));
  } else {
    RejectJavascriptCallback(*callback_id,
                             base::Value("No prefetch service available."));
  }
}

void OfflineInternalsUIMessageHandler::HandleShowPrefetchNotification(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  offline_pages::ShowPrefetchedContentNotification(
      GURL("https://www.example.com"));
  ResolveJavascriptCallback(*callback_id, base::Value("Scheduled."));
}

void OfflineInternalsUIMessageHandler::HandleGeneratePageBundle(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (!prefetch_service_) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("No prefetch service available."));
    return;
  }

  std::string data;
  CHECK(args->GetString(1, &data));
  std::vector<std::string> page_urls = base::SplitStringUsingSubstr(
      data, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<offline_pages::PrefetchURL> prefetch_urls;
  for (auto& page_url : page_urls) {
    // Creates a dummy prefetch URL with a bogus ID, and using the URL as the
    // page title.
    prefetch_urls.push_back(offline_pages::PrefetchURL(
        "dummy id", GURL(page_url), base::UTF8ToUTF16(page_url)));
  }

  prefetch_service_->GetPrefetchDispatcher()->AddCandidatePrefetchURLs(
      offline_pages::kSuggestedArticlesNamespace, prefetch_urls);
  std::string message("Added candidate URLs.\n");
  // Construct a JSON array containing all the URLs. To guard against malicious
  // URLs that might contain special characters, we create a ListValue and then
  // serialize it into JSON, instead of doing direct string manipulation.
  base::ListValue urls;
  for (const auto& prefetch_url : prefetch_urls) {
    urls.GetList().emplace_back(prefetch_url.url.spec());
  }
  std::string json;
  base::JSONWriter::Write(urls, &json);
  message.append(json);
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(message));
}

void OfflineInternalsUIMessageHandler::HandleGetOperation(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (!prefetch_service_) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("No prefetch service available."));
    return;
  }

  std::string name;
  CHECK(args->GetString(1, &name));
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);

  prefetch_service_->GetPrefetchDispatcher()
      ->GCMOperationCompletedMessageReceived(name);
  base::Value message("GetOperation will be attempted for any matching pages.");
  ResolveJavascriptCallback(base::Value(callback_id), message);
}

void OfflineInternalsUIMessageHandler::HandleDownloadArchive(
    const base::ListValue* args) {
  AllowJavascript();
  std::string name;
  CHECK(args->GetString(0, &name));
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);

  if (prefetch_service_) {
    prefetch_service_->GetPrefetchDownloader()->StartDownload(
        base::GenerateGUID(), name);
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue(
    const base::ListValue* args) {
  AllowJavascript();
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  if (request_coordinator_)
    request_coordinator_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPrefetchService(
    const base::ListValue* args) {
  AllowJavascript();
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  if (prefetch_service_)
    prefetch_service_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetLoggingState(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::DictionaryValue result;
  result.SetBoolean("modelIsLogging",
                    offline_page_model_
                        ? offline_page_model_->GetLogger()->GetIsLogging()
                        : false);
  result.SetBoolean("queueIsLogging",
                    request_coordinator_
                        ? request_coordinator_->GetLogger()->GetIsLogging()
                        : false);
  bool prefetch_logging = false;
  if (prefetch_service_) {
    prefetch_logging = prefetch_service_->GetLogger()->GetIsLogging();
  }
  result.SetBoolean("prefetchIsLogging", prefetch_logging);
  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleGetEventLogs(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::vector<std::string> logs;
  if (offline_page_model_)
    offline_page_model_->GetLogger()->GetLogs(&logs);
  if (request_coordinator_)
    request_coordinator_->GetLogger()->GetLogs(&logs);
  if (prefetch_service_)
    prefetch_service_->GetLogger()->GetLogs(&logs);
  std::sort(logs.begin(), logs.end());

  base::ListValue result;
  result.AppendStrings(logs);

  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleAddToRequestQueue(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (request_coordinator_) {
    std::string url;
    CHECK(args->GetString(1, &url));

    // To be visible in Downloads UI, these items need a well-formed GUID
    // and AsyncNamespace in their ClientId.
    std::ostringstream id_stream;
    id_stream << base::GenerateGUID();

    offline_pages::RequestCoordinator::SavePageLaterParams params;
    params.url = GURL(url);
    params.client_id = offline_pages::ClientId(offline_pages::kAsyncNamespace,
                                               id_stream.str());
    request_coordinator_->SavePageLater(
        params,
        base::Bind(
            &OfflineInternalsUIMessageHandler::HandleSavePageLaterCallback,
            weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(false));
  }
}

void OfflineInternalsUIMessageHandler::HandleSavePageLaterCallback(
    std::string callback_id,
    offline_pages::AddRequestResult result) {
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(result == offline_pages::AddRequestResult::SUCCESS));
}

void OfflineInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "deleteSelectedPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteSelectedRequests",
      base::Bind(
          &OfflineInternalsUIMessageHandler::HandleDeleteSelectedRequests,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetRequestQueue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStoredPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetStoredPages,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEventLogs",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetEventLogs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setRecordRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setRecordPageModel",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordPageModel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setRecordPrefetchService",
      base::Bind(
          &OfflineInternalsUIMessageHandler::HandleSetRecordPrefetchService,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLoggingState",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetLoggingState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addToRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleAddToRequestQueue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNetworkStatus",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetNetworkStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "scheduleNwake",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleScheduleNwake,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelNwake",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleCancelNwake,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showPrefetchNotification",
      base::Bind(
          &OfflineInternalsUIMessageHandler::HandleShowPrefetchNotification,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "generatePageBundle",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGeneratePageBundle,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getOperation",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetOperation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "downloadArchive",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDownloadArchive,
                 base::Unretained(this)));

  // Get the offline page model associated with this web ui.
  Profile* profile = Profile::FromWebUI(web_ui());
  offline_page_model_ =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile);
  request_coordinator_ =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(profile);
  prefetch_service_ =
      offline_pages::PrefetchServiceFactory::GetForBrowserContext(profile);
}

void OfflineInternalsUIMessageHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace offline_internals
