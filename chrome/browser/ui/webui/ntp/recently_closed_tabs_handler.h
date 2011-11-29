// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_RECENTLY_CLOSED_TABS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_RECENTLY_CLOSED_TABS_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "content/browser/webui/web_ui.h"

class TabRestoreService;

class RecentlyClosedTabsHandler : public WebUIMessageHandler,
                                  public TabRestoreServiceObserver {
 public:
  RecentlyClosedTabsHandler() : tab_restore_service_(NULL) {}
  virtual ~RecentlyClosedTabsHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "reopenTab" message. Rewrites the history of the
  // currently displayed tab to be the one in TabRestoreService with a
  // history of a session passed in through the content pointer.
  void HandleReopenTab(const ListValue* args);

  // Callback for the "getRecentlyClosedTabs" message.
  void HandleGetRecentlyClosedTabs(const ListValue* args);

  // Observer callback for TabRestoreServiceObserver. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

  // Converts a list of TabRestoreService entries to the JSON format required
  // by the NTP and adds them to the given list value.
  static void CreateRecentlyClosedValues(
      const TabRestoreService::Entries& entries,
      base::ListValue* entry_list_value);

 private:
  // TabRestoreService that we are observing.
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyClosedTabsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_RECENTLY_CLOSED_TABS_HANDLER_H_
