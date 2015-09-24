// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_DELEGATE_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {

class LiveTab;
class SerializedNavigationEntry;
class PlatformSpecificTabData;

// Objects implement this interface to provide necessary functionality for
// TabRestoreService to operate. These methods are mostly copies of existing
// Browser methods.
class SESSIONS_EXPORT TabRestoreServiceDelegate {
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

  virtual LiveTab* GetLiveTabAt(int index) const = 0;
  virtual LiveTab* GetActiveLiveTab() const = 0;
  virtual bool IsTabPinned(int index) const = 0;

  // Note: |tab_platform_data| may be null (e.g., if |from_last_session| is
  // true, as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  virtual LiveTab* AddRestoredTab(
      const std::vector<SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      const PlatformSpecificTabData* tab_platform_data,
      const std::string& user_agent_override) = 0;

  // Note: |tab_platform_data| may be null (e.g., if |from_last_session| is
  // true, as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  virtual LiveTab* ReplaceRestoredTab(
      const std::vector<SerializedNavigationEntry>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const PlatformSpecificTabData* tab_platform_data,
      const std::string& user_agent_override) = 0;
  virtual void CloseTab() = 0;

 protected:
  virtual ~TabRestoreServiceDelegate() {}
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_DELEGATE_H_
