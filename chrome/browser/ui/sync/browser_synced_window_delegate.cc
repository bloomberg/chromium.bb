// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"

#include <set>

#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

// static SyncedWindowDelegate implementations

// static
const std::set<browser_sync::SyncedWindowDelegate*>
    browser_sync::SyncedWindowDelegate::GetSyncedWindowDelegates() {
  std::set<browser_sync::SyncedWindowDelegate*> synced_window_delegates;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    synced_window_delegates.insert((*i)->synced_window_delegate());
  }
  return synced_window_delegates;
}

// static
const browser_sync::SyncedWindowDelegate*
    browser_sync::SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
        SessionID::id_type id) {
  Browser* browser = chrome::FindBrowserWithID(id);
  // In case we don't find the browser (e.g. for Developer Tools).
  return browser ? browser->synced_window_delegate() : NULL;
}

// BrowserSyncedWindowDelegate implementations

BrowserSyncedWindowDelegate::BrowserSyncedWindowDelegate(Browser* browser)
    : browser_(browser) {}

BrowserSyncedWindowDelegate::~BrowserSyncedWindowDelegate() {}

bool BrowserSyncedWindowDelegate::IsTabPinned(
    const browser_sync::SyncedTabDelegate* tab) const {
  for (int i = 0; i < browser_->tab_strip_model()->count(); i++) {
    browser_sync::SyncedTabDelegate* current = GetTabAt(i);
    if (tab == current)
      return browser_->tab_strip_model()->IsTabPinned(i);
  }
  NOTREACHED();
  return false;
}

browser_sync::SyncedTabDelegate* BrowserSyncedWindowDelegate::GetTabAt(
    int index) const {
  return TabContentsSyncedTabDelegate::FromWebContents(
      browser_->tab_strip_model()->GetWebContentsAt(index));
}

SessionID::id_type BrowserSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool BrowserSyncedWindowDelegate::HasWindow() const {
  return browser_->window() != NULL;
}

SessionID::id_type BrowserSyncedWindowDelegate::GetSessionId() const {
  return browser_->session_id().id();
}

int BrowserSyncedWindowDelegate::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

int BrowserSyncedWindowDelegate::GetActiveIndex() const {
  return browser_->tab_strip_model()->active_index();
}

bool BrowserSyncedWindowDelegate::IsApp() const {
  return browser_->is_app();
}

bool BrowserSyncedWindowDelegate::IsTypeTabbed() const {
  return browser_->is_type_tabbed();
}

bool BrowserSyncedWindowDelegate::IsTypePopup() const {
  return browser_->is_type_popup();
}

bool BrowserSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  return false;
}
