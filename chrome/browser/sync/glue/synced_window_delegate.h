// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_H_

#include <set>

#include "components/sessions/session_id.h"

namespace browser_sync {

class SyncedTabDelegate;

// A SyncedWindowDelegate is used to insulate the sync code from depending
// directly on Browser and BrowserList.
class SyncedWindowDelegate {
 public:
  // Methods originating from WindowList

  // This method is to be used instead of using the BrowserList iterator.
  static const std::set<SyncedWindowDelegate*> GetSyncedWindowDelegates();

  // This method is to be used instead of using BrowserList::FindBrowserWithId()
  static const SyncedWindowDelegate* FindSyncedWindowDelegateWithId(
      SessionID::id_type id);

  // Methods originating from Browser.

  // Returns true iff this browser has a visible window representation
  // associated with it. Sometimes, if a window is being created/removed the
  // model object may exist without its UI counterpart.
  virtual bool HasWindow() const = 0;

  // see Browser::session_id
  virtual SessionID::id_type GetSessionId() const = 0;

  // see Browser::tab_count
  virtual int GetTabCount() const = 0;

  // see Browser::active_index
  virtual int GetActiveIndex() const = 0;

  // see Browser::is_app
  virtual bool IsApp() const = 0;

  // see Browser::is_type_tabbed
  virtual bool IsTypeTabbed() const = 0;

  // see Browser::is_type_popup
  virtual bool IsTypePopup() const = 0;

  // Methods derivated from Browser

  // Returns true iff the provided tab is currently "pinned" in the tab strip.
  virtual bool IsTabPinned(const SyncedTabDelegate* tab) const = 0;

  // see TabStripModel::GetWebContentsAt
  virtual SyncedTabDelegate* GetTabAt(int index) const = 0;

  // Return the tab id for the tab at |index|.
  virtual SessionID::id_type GetTabIdAt(int index) const = 0;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const = 0;

 protected:
  virtual ~SyncedWindowDelegate() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_H_
