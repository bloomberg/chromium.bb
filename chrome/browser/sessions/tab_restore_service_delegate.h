// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "chrome/browser/sessions/session_id.h"

class Profile;
class SessionStorageNamespace;
class TabContents;
class TabNavigation;

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

  // see Browser::selected_index()
  virtual int GetSelectedIndex() const = 0;

  // see Browser methods with the same names
  virtual TabContents* GetTabContentsAt(int index) const = 0;
  virtual TabContents* GetSelectedTabContents() const = 0;
  virtual bool IsTabPinned(int index) const = 0;
  virtual TabContents* AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      SessionStorageNamespace* storage_namespace) = 0;
  virtual void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      SessionStorageNamespace* session_storage_namespace) = 0;
  virtual void CloseTab() = 0;

  // see Browser::Create
  static TabRestoreServiceDelegate* Create(Profile* profile);

  // see BrowserList::GetBrowserForController
  static TabRestoreServiceDelegate* FindDelegateForController(
      const NavigationController* controller,
      int* index);

  // see BrowserList::FindBrowserWithID
  static TabRestoreServiceDelegate* FindDelegateWithID(
      SessionID::id_type desired_id);

 protected:
  virtual ~TabRestoreServiceDelegate() {}
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_H_
