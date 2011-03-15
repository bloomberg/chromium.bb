// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"

class Browser;
class Profile;
class SessionStorageNamespace;
class TabContents;
class TabNavigation;

// Implementation of TabRestoreServiceDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserTabRestoreServiceDelegate : public TabRestoreServiceDelegate {
 public:
  explicit BrowserTabRestoreServiceDelegate(Browser* browser)
      : browser_(browser) {}
  virtual ~BrowserTabRestoreServiceDelegate() {}

  // Overridden from TabRestoreServiceDelegate:
  virtual void ShowBrowserWindow() OVERRIDE;
  virtual const SessionID& GetSessionID() const OVERRIDE;
  virtual int GetTabCount() const OVERRIDE;
  virtual int GetSelectedIndex() const OVERRIDE;
  virtual TabContents* GetTabContentsAt(int index) const OVERRIDE;
  virtual TabContents* GetSelectedTabContents() const OVERRIDE;
  virtual bool IsTabPinned(int index) const OVERRIDE;
  virtual TabContents* AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      SessionStorageNamespace* storage_namespace) OVERRIDE;
  virtual void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      SessionStorageNamespace* session_storage_namespace) OVERRIDE;
  virtual void CloseTab() OVERRIDE;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabRestoreServiceDelegate);
};

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_RESTORE_SERVICE_DELEGATE_H_
