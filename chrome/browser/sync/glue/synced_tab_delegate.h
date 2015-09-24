// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_driver/sessions/synced_window_delegates_getter.h"

class Profile;

namespace content {
class NavigationEntry;
class WebContents;
}

namespace browser_sync {
class SyncedWindowDelegate;

// A SyncedTabDelegate is used to insulate the sync code from depending
// directly on WebContents, NavigationController, and the extensions TabHelper.
class SyncedTabDelegate {
 public:
  virtual ~SyncedTabDelegate();

  // Methods from TabContents.

  virtual SessionID::id_type GetWindowId() const = 0;
  virtual SessionID::id_type GetSessionId() const = 0;
  virtual bool IsBeingDestroyed() const = 0;
  virtual Profile* profile() const = 0;

  // Method derived from extensions TabHelper.

  virtual std::string GetExtensionAppId() const = 0;

  // Methods from NavigationController.

  virtual int GetCurrentEntryIndex() const = 0;
  virtual int GetEntryCount() const = 0;
  virtual int GetPendingEntryIndex() const = 0;
  virtual content::NavigationEntry* GetPendingEntry() const = 0;
  virtual content::NavigationEntry* GetEntryAtIndex(int i) const = 0;
  virtual content::NavigationEntry* GetActiveEntry() const = 0;

  // The idea here is that GetEntryAtIndex may not always return the pending
  // entry when asked for the entry at the pending index. These convinience
  // methods will check for this case and then call the correct entry accessor.
  content::NavigationEntry* GetCurrentEntryMaybePending() const;
  content::NavigationEntry* GetEntryAtIndexMaybePending(int i) const;

  // Supervised user related methods.

  virtual bool ProfileIsSupervised() const = 0;
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const = 0;

  virtual bool IsPinned() const = 0;
  virtual bool HasWebContents() const = 0;
  virtual content::WebContents* GetWebContents() const = 0;

  // Session sync related methods.
  virtual int GetSyncId() const = 0;
  virtual void SetSyncId(int sync_id) = 0;

  // Returns true if this tab should be synchronized.
  bool ShouldSync() const;

  // Sets the window getter. This must be called before any of the sync or
  // supervised user methods on this class are called. It is currently set when
  // the SyncTabDelegate is retrieved from WebContents via ImplFromWebContents.
  void SetSyncedWindowGetter(scoped_ptr<SyncedWindowDelegatesGetter> getter);

  // Returns the SyncedTabDelegate associated with WebContents.
  static SyncedTabDelegate* ImplFromWebContents(
      content::WebContents* web_contents);

 protected:
  SyncedTabDelegate();

  // Overridden by the tests to avoid interaction with static state.
  virtual const SyncedWindowDelegate* GetSyncedWindowDelegate() const;

 private:
  // A getter for accessing the associated SyncedWindowDelegate.
  scoped_ptr<SyncedWindowDelegatesGetter> synced_window_getter_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__
