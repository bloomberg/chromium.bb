// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "content/browser/cancelable_request.h"
#include "content/common/notification_registrar.h"

class GURL;

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public WebUIMessageHandler,
                               public NotificationObserver {
 public:
  BrowsingHistoryHandler();
  virtual ~BrowsingHistoryHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "getHistory" message.
  void HandleGetHistory(const base::ListValue* args);

  // Callback for the "searchHistory" message.
  void HandleSearchHistory(const base::ListValue* args);

  // Callback for the "removeURLsOnOneDay" message.
  void HandleRemoveURLsOnOneDay(const base::ListValue* args);

  // Handle for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Callback from the history system when the history list is available.
  void QueryComplete(HistoryService::Handle request_handle,
                     history::QueryResults* results);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // Extract the arguments from the call to HandleSearchHistory.
  void ExtractSearchHistoryArguments(const base::ListValue* args,
                                     int* month,
                                     string16* query);

  // Figure out the query options for a month-wide query.
  history::QueryOptions CreateMonthQueryOptions(int month);

  NotificationRegistrar registrar_;

  // Current search text.
  string16 search_text_;

  // Our consumer for search requests to the history service.
  CancelableRequestConsumerT<int, 0> cancelable_search_consumer_;

  // Our consumer for delete requests to the history service.
  CancelableRequestConsumerT<int, 0> cancelable_delete_consumer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

class HistoryUI : public ChromeWebUI {
 public:
  explicit HistoryUI(TabContents* contents);

  // Return the URL for a given search term.
  static const GURL GetHistoryURLWithSearchText(const string16& text);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
