// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"

class Browser;

// Implementation of TabRestoreServiceDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserTabRestoreServiceDelegate : public TabRestoreServiceDelegate {
 public:
  explicit BrowserTabRestoreServiceDelegate(Browser* browser)
      : browser_(browser) {}
  ~BrowserTabRestoreServiceDelegate() override {}

  // Overridden from TabRestoreServiceDelegate:
  void ShowBrowserWindow() override;
  const SessionID& GetSessionID() const override;
  int GetTabCount() const override;
  int GetSelectedIndex() const override;
  std::string GetAppName() const override;
  content::WebContents* GetWebContentsAt(int index) const override;
  content::WebContents* GetActiveWebContents() const override;
  bool IsTabPinned(int index) const override;
  content::WebContents* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      content::SessionStorageNamespace* storage_namespace,
      const std::string& user_agent_override) override;
  content::WebContents* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      content::SessionStorageNamespace* session_storage_namespace,
      const std::string& user_agent_override) override;
  void CloseTab() override;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabRestoreServiceDelegate);
};

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_
