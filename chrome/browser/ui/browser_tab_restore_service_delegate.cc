// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"

using content::NavigationController;
using content::SessionStorageNamespace;
using content::WebContents;

void BrowserTabRestoreServiceDelegate::ShowBrowserWindow() {
  browser_->window()->Show();
}

const SessionID& BrowserTabRestoreServiceDelegate::GetSessionID() const {
  return browser_->session_id();
}

int BrowserTabRestoreServiceDelegate::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

int BrowserTabRestoreServiceDelegate::GetSelectedIndex() const {
  return browser_->tab_strip_model()->active_index();
}

std::string BrowserTabRestoreServiceDelegate::GetAppName() const {
  return browser_->app_name();
}

SessionAppType BrowserTabRestoreServiceDelegate::GetAppType() const {
  return browser_->app_type() == Browser::APP_TYPE_HOST ?
      SESSION_APP_TYPE_HOST : SESSION_APP_TYPE_CHILD;
}

WebContents* BrowserTabRestoreServiceDelegate::GetWebContentsAt(
    int index) const {
  return browser_->tab_strip_model()->GetWebContentsAt(index);
}

WebContents* BrowserTabRestoreServiceDelegate::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool BrowserTabRestoreServiceDelegate::IsTabPinned(int index) const {
  return browser_->tab_strip_model()->IsTabPinned(index);
}

WebContents* BrowserTabRestoreServiceDelegate::AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      SessionStorageNamespace* storage_namespace,
      const std::string& user_agent_override) {
  return chrome::AddRestoredTab(browser_, navigations, tab_index,
                                selected_navigation, extension_app_id, select,
                                pin, from_last_session, storage_namespace,
                                user_agent_override);
}

void BrowserTabRestoreServiceDelegate::ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      SessionStorageNamespace* session_storage_namespace,
      const std::string& user_agent_override) {
  chrome::ReplaceRestoredTab(browser_, navigations, selected_navigation,
                             from_last_session, extension_app_id,
                             session_storage_namespace, user_agent_override);
}

void BrowserTabRestoreServiceDelegate::CloseTab() {
  chrome::CloseTab(browser_);
}

// Implementations of TabRestoreServiceDelegate static methods

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::Create(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    const std::string& app_name,
    SessionAppType app_type) {
  Browser* browser;
  if (app_name.empty()) {
    browser = new Browser(Browser::CreateParams(profile, host_desktop_type));
  } else {
    Browser::CreateParams params(Browser::TYPE_POPUP,
                                 profile,
                                 host_desktop_type);
    params.app_name = app_name;
    params.app_type = app_type ==
        SESSION_APP_TYPE_CHILD ?
            Browser::APP_TYPE_CHILD : Browser::APP_TYPE_HOST;
    params.initial_bounds = gfx::Rect();
    browser = new Browser(params);
  }
  if (browser)
    return browser->tab_restore_service_delegate();
  else
    return NULL;
}

// static
TabRestoreServiceDelegate*
    TabRestoreServiceDelegate::FindDelegateForWebContents(
        const WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  return browser ? browser->tab_restore_service_delegate() : NULL;
}

// static
TabRestoreServiceDelegate* TabRestoreServiceDelegate::FindDelegateWithID(
    SessionID::id_type desired_id,
    chrome::HostDesktopType host_desktop_type) {
  Browser* browser = chrome::FindBrowserWithID(desired_id);
  return (browser && browser->host_desktop_type() == host_desktop_type) ?
             browser->tab_restore_service_delegate() : NULL;
}
