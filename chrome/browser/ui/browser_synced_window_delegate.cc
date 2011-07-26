// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_synced_window_delegate.h"

#include <set>

#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

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
  return BrowserList::FindBrowserWithID(id)->synced_window_delegate();
}

// BrowserSyncedWindowDelegate implementations

BrowserSyncedWindowDelegate::BrowserSyncedWindowDelegate(Browser* browser)
    : browser_(browser) {}

BrowserSyncedWindowDelegate::~BrowserSyncedWindowDelegate() {}

bool BrowserSyncedWindowDelegate::IsTabContentsWrapperPinned(
    const TabContentsWrapper* tab) const {
  int index = browser_->GetIndexOfController(&tab->controller());
  DCHECK(index != TabStripModel::kNoTab);
  return browser_->tabstrip_model()->IsTabPinned(index);
}

TabContentsWrapper* BrowserSyncedWindowDelegate::GetTabContentsWrapperAt(
    int index) const {
  return browser_->GetTabContentsWrapperAt(index);
}

bool BrowserSyncedWindowDelegate::HasWindow() const {
  return browser_->window() != NULL;
}

const SessionID& BrowserSyncedWindowDelegate::GetSessionId() const {
  return browser_->session_id();
}

int BrowserSyncedWindowDelegate::GetTabCount() const {
  return browser_->tab_count();
}

int BrowserSyncedWindowDelegate::GetActiveIndex() const {
  return browser_->active_index();
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
