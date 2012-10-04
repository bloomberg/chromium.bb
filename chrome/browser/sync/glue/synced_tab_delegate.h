// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__

#include <string>

#include "chrome/browser/sessions/session_id.h"

class Profile;

namespace content {
class NavigationEntry;
}

namespace browser_sync {

// A SyncedTabDelegate is used to insulate the sync code from depending
// directly on WebContents, NavigationController, and the extensions TabHelper.
class SyncedTabDelegate {
 public:
  virtual ~SyncedTabDelegate() {}

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

  virtual bool IsPinned() const = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_H__
