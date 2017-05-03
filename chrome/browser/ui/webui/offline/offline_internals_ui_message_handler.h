// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace offline_pages {
enum class GetRequestsResult;
}

namespace offline_internals {

// Class acting as a controller of the chrome://offline-internals WebUI.
class OfflineInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  OfflineInternalsUIMessageHandler();
  ~OfflineInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Delete selected list of page ids from the store.
  void HandleDeleteSelectedPages(const base::ListValue* args);

  // Delete selected list of requests from the request queue.
  void HandleDeleteSelectedRequests(const base::ListValue* args);

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

  // Adds a url to the background loader queue.
  void HandleAddToRequestQueue(const base::ListValue* args);

  // Load whether device is currently offline.
  void HandleGetNetworkStatus(const base::ListValue* args);

  // Schedules an NWake signal.
  void HandleScheduleNwake(const base::ListValue* args);

  // Cancels an NWake signal.
  void HandleCancelNwake(const base::ListValue* args);

  // Callback for async GetAllPages calls.
  void HandleStoredPagesCallback(
      std::string callback_id,
      const offline_pages::MultipleOfflinePageItemResult& pages);

  // Callback for async GetRequests calls.
  void HandleRequestQueueCallback(
      std::string callback_id,
      offline_pages::GetRequestsResult result,
      std::vector<std::unique_ptr<offline_pages::SavePageRequest>> requests);

  // Callback for DeletePage/DeleteAllPages calls.
  void HandleDeletedPagesCallback(std::string callback_id,
                                  const offline_pages::DeletePageResult result);

  // Callback for DeleteRequest/DeleteAllRequests calls.
  void HandleDeletedRequestsCallback(
      std::string callback_id,
      const offline_pages::MultipleItemStatuses& results);

  // Turns a DeletePageResult enum into logical string.
  std::string GetStringFromDeletePageResult(
      offline_pages::DeletePageResult value);

  // Summarizes the MultipleItemStatuses vector with a string.
  std::string GetStringFromDeleteRequestResults(
      const offline_pages::MultipleItemStatuses& result);

  // Turns a SavePageRequest::Status into logical string.
  std::string GetStringFromSavePageStatus();

  // Offline page model to call methods on.
  offline_pages::OfflinePageModel* offline_page_model_;

  // Request coordinator for background offline actions.
  offline_pages::RequestCoordinator* request_coordinator_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<OfflineInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflineInternalsUIMessageHandler);
};

}  // namespace offline_internals

#endif // CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_
