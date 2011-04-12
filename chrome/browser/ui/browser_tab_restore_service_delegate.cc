// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

void BrowserTabRestoreServiceDelegate::ShowBrowserWindow() {
  browser_->window()->Show();
}

const SessionID& BrowserTabRestoreServiceDelegate::GetSessionID() const {
  return browser_->session_id();
}

int BrowserTabRestoreServiceDelegate::GetTabCount() const {
  return browser_->tab_count();
}

int BrowserTabRestoreServiceDelegate::GetSelectedIndex() const {
  return browser_->active_index();
}

TabContents* BrowserTabRestoreServiceDelegate::GetTabContentsAt(
    int index) const {
  return browser_->GetTabContentsAt(index);
}

TabContents* BrowserTabRestoreServiceDelegate::GetSelectedTabContents() const {
  return browser_->GetSelectedTabContents();
}

bool BrowserTabRestoreServiceDelegate::IsTabPinned(int index) const {
  return browser_->IsTabPinned(index);
}

TabContents* BrowserTabRestoreServiceDelegate::AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      SessionStorageNamespace* storage_namespace) {
  return browser_->AddRestoredTab(navigations, tab_index, selected_navigation,
                                  extension_app_id, select, pin,
                                  from_last_session, storage_namespace);
}

void BrowserTabRestoreServiceDelegate::ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      SessionStorageNamespace* session_storage_namespace) {
  browser_->ReplaceRestoredTab(navigations, selected_navigation,
                               from_last_session, extension_app_id,
                               session_storage_namespace);
}

void BrowserTabRestoreServiceDelegate::CloseTab() {
  browser_->CloseTab();
}

// Implementations of TabRestoreServiceDelegate static methods

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::Create(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  if (browser)
    return browser->tab_restore_service_delegate();
  else
    return NULL;
}

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::FindDelegateForController(
    const NavigationController* controller,
    int* index) {
  Browser* browser = Browser::GetBrowserForController(controller, index);
  if (browser)
    return browser->tab_restore_service_delegate();
  else
    return NULL;
}

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::FindDelegateWithID(
    SessionID::id_type desired_id) {
  Browser* browser = BrowserList::FindBrowserWithID(desired_id);
  if (browser)
    return browser->tab_restore_service_delegate();
  else
    return NULL;
}
