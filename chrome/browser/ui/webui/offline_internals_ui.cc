// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline_internals_ui.h"

#include <stdint.h>
#include <stdlib.h>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

// Class acting as a controller of the chrome://offline-internals WebUI.
class OfflineInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  OfflineInternalsUIMessageHandler();
  ~OfflineInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Deletes all the pages in the store.
  void HandleDeleteAllPages(const base::ListValue* args);

  // Delete selected list of page ids from the store.
  void HandleDeleteSelectedPages(const base::ListValue* args);

  // Load Request Queue info.
  void HandleGetRequestQueue(const base::ListValue* args);

  // Load Stored pages info.
  void HandleGetStoredPages(const base::ListValue* args);

  // Set whether to record offline page model events.
  void HandleSetRecordPageModel(const base::ListValue* args);

  // Set whether to record request queue events.
  void HandleSetRecordRequestQueue(const base::ListValue* args);

  // Load both Page Model and Request Queue event logs.
  void HandleGetEventLogs(const base::ListValue* args);

  // Load whether logs are being recorded.
  void HandleGetLoggingState(const base::ListValue* args);

  // Callback for async GetAllPages calls.
  void HandleStoredPagesCallback(
      std::string callback_id,
      const offline_pages::MultipleOfflinePageItemResult& pages);

  // Callback for async GetRequests calls.
  void HandleRequestQueueCallback(
      std::string callback_id,
      offline_pages::RequestQueue::GetRequestsResult result,
      const std::vector<offline_pages::SavePageRequest>& requests);

  // Callback for DeletePage/ClearAll calls.
  void HandleDeletedPagesCallback(std::string callback_id,
                                  const offline_pages::DeletePageResult result);

  // Turns a DeletePageResult enum into logical string.
  std::string GetStringFromDeletePageResult(
      offline_pages::DeletePageResult value);

  // Turns a SavePageRequest::Status into logical string.
  std::string GetStringFromSavePageStatus(
      offline_pages::SavePageRequest::Status status);

  // Offline page model to call methods on.
  offline_pages::OfflinePageModel* offline_page_model_;

  // Request coordinator for background offline actions.
  offline_pages::RequestCoordinator* request_coordinator_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<OfflineInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflineInternalsUIMessageHandler);
};

OfflineInternalsUIMessageHandler::OfflineInternalsUIMessageHandler()
    : offline_page_model_(nullptr),
      request_coordinator_(nullptr),
      weak_ptr_factory_(this) {}

OfflineInternalsUIMessageHandler::~OfflineInternalsUIMessageHandler() {}

std::string OfflineInternalsUIMessageHandler::GetStringFromDeletePageResult(
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

std::string OfflineInternalsUIMessageHandler::GetStringFromSavePageStatus(
    offline_pages::SavePageRequest::Status status) {
  switch (status) {
    case offline_pages::SavePageRequest::Status::NOT_READY:
      return "Not ready";
    case offline_pages::SavePageRequest::Status::PENDING:
      return "Pending";
    case offline_pages::SavePageRequest::Status::STARTED:
      return "Started";
    case offline_pages::SavePageRequest::Status::FAILED:
      return "Failed";
    case offline_pages::SavePageRequest::Status::EXPIRED:
      return "Expired";
  }
  NOTREACHED();
  return "Unknown";
}

void OfflineInternalsUIMessageHandler::HandleDeleteAllPages(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  // Pass back success because ClearAll doesn't return a status.
  offline_page_model_->ClearAll(
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id,
                 offline_pages::DeletePageResult::SUCCESS));
}

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

void OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback(
    std::string callback_id,
    offline_pages::DeletePageResult result) {
  ResolveJavascriptCallback(
      base::StringValue(callback_id),
      base::StringValue(GetStringFromDeletePageResult(result)));
}

void OfflineInternalsUIMessageHandler::HandleStoredPagesCallback(
    std::string callback_id,
    const offline_pages::MultipleOfflinePageItemResult& pages) {
  base::ListValue results;

  for (const auto& page : pages) {
    base::DictionaryValue* offline_page = new base::DictionaryValue();
    results.Append(offline_page);
    offline_page->SetString("onlineUrl", page.url.spec());
    offline_page->SetString("namespace", page.client_id.name_space);
    offline_page->SetDouble("size", page.file_size);
    offline_page->SetString("id", std::to_string(page.offline_id));
    offline_page->SetString("filePath", page.file_path.value());
    offline_page->SetDouble("creationTime", page.creation_time.ToJsTime());
    offline_page->SetDouble("lastAccessTime",
                              page.last_access_time.ToJsTime());
    offline_page->SetInteger("accessCount", page.access_count);
  }
  ResolveJavascriptCallback(base::StringValue(callback_id), results);
}

void OfflineInternalsUIMessageHandler::HandleRequestQueueCallback(
    std::string callback_id,
    offline_pages::RequestQueue::GetRequestsResult result,
    const std::vector<offline_pages::SavePageRequest>& requests) {
  base::ListValue save_page_requests;
  if (result == offline_pages::RequestQueue::GetRequestsResult::SUCCESS) {
    for (const auto& request : requests) {
      base::DictionaryValue* save_page_request = new base::DictionaryValue();
      save_page_requests.Append(save_page_request);
      save_page_request->SetString("onlineUrl", request.url().spec());
      save_page_request->SetDouble("creationTime",
                                   request.creation_time().ToJsTime());
      save_page_request->SetString(
          "status",
          GetStringFromSavePageStatus(request.GetStatus(base::Time::Now())));
      save_page_request->SetString("namespace", request.client_id().name_space);
      save_page_request->SetDouble("lastAttempt",
                                   request.last_attempt_time().ToJsTime());
      save_page_request->SetString("id", std::to_string(request.request_id()));
    }
  }
  ResolveJavascriptCallback(base::StringValue(callback_id), save_page_requests);
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
    ResolveJavascriptCallback(base::StringValue(callback_id), results);
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
    ResolveJavascriptCallback(base::StringValue(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPageModel(
    const base::ListValue* args) {
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  offline_page_model_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue(
    const base::ListValue* args) {
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  request_coordinator_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetLoggingState(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::DictionaryValue result;
  result.SetBoolean("modelIsLogging",
                    offline_page_model_->GetLogger()->GetIsLogging());
  result.SetBoolean("queueIsLogging",
                    request_coordinator_->GetLogger()->GetIsLogging());
  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleGetEventLogs(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::vector<std::string> logs;
  offline_page_model_->GetLogger()->GetLogs(&logs);
  request_coordinator_->GetLogger()->GetLogs(&logs);
  std::sort(logs.begin(), logs.end());

  base::ListValue result;
  result.AppendStrings(logs);

  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "deleteAllPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteAllPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deleteSelectedPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetRequestQueue,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getStoredPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetStoredPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEventLogs",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetEventLogs,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setRecordRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setRecordPageModel",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordPageModel,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getLoggingState",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetLoggingState,
                 weak_ptr_factory_.GetWeakPtr()));

  // Get the offline page model associated with this web ui.
  Profile* profile = Profile::FromWebUI(web_ui());
  offline_page_model_ =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile);
  request_coordinator_ =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(profile);
}

}  // namespace

OfflineInternalsUI::OfflineInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // chrome://offline-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOfflineInternalsHost);

  // Required resources.
  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("offline_internals.css",
                               IDR_OFFLINE_INTERNALS_CSS);
  html_source->AddResourcePath("offline_internals.js",
                               IDR_OFFLINE_INTERNALS_JS);
  html_source->AddResourcePath("offline_internals_browser_proxy.js",
                               IDR_OFFLINE_INTERNALS_BROWSER_PROXY_JS);
  html_source->SetDefaultResource(IDR_OFFLINE_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  web_ui->AddMessageHandler(new OfflineInternalsUIMessageHandler());
}

OfflineInternalsUI::~OfflineInternalsUI() {}
