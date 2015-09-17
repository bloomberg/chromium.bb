// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "components/sessions/session_id.h"

namespace sessions {
class LiveTab;
}

namespace sessions {
class SerializedNavigationEntry;
class TabClientData;
}

// Objects implement this interface to provide necessary functionality for
// TabRestoreService to operate. These methods are mostly copies of existing
// Browser methods.
class TabRestoreServiceDelegate {
 public:
  // see BrowserWindow::Show()
  virtual void ShowBrowserWindow() = 0;

  // see Browser::session_id()
  virtual const SessionID& GetSessionID() const = 0;

  // see Browser::tab_count()
  virtual int GetTabCount() const = 0;

  // see Browser::active_index()
  virtual int GetSelectedIndex() const = 0;

  // see Browser::app_name()
  virtual std::string GetAppName() const = 0;

  virtual sessions::LiveTab* GetLiveTabAt(int index) const = 0;
  virtual sessions::LiveTab* GetActiveLiveTab() const = 0;
  virtual bool IsTabPinned(int index) const = 0;

  // Note: |tab_client_data| may be null (e.g., if |from_last_session| is true,
  // as the tab client data is not persisted, or if the embedder did not supply
  // client data for the tab in question).
  virtual sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      const sessions::TabClientData* tab_client_data,
      const std::string& user_agent_override) = 0;

  // Note: |tab_client_data| may be null (e.g., if |from_last_session| is true,
  // as the tab client data is not persisted, or if the embedder did not supply
  virtual sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const sessions::TabClientData* tab_client_data,
      const std::string& user_agent_override) = 0;
  virtual void CloseTab() = 0;

 protected:
  virtual ~TabRestoreServiceDelegate() {}
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_
