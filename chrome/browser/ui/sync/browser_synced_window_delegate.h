// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "components/sessions/session_id.h"

class Browser;

namespace browser_sync {
class SyncedTabDelegate;
}

// A BrowserSyncedWindowDelegate is the Browser-based implementation of
// SyncedWindowDelegate.
class BrowserSyncedWindowDelegate : public browser_sync::SyncedWindowDelegate {
 public:
  explicit BrowserSyncedWindowDelegate(Browser* browser);
  ~BrowserSyncedWindowDelegate() override;

  // Add all BrowserSyncedWindowDelegate instances to the output set.
  static void GetAllHelper(
      std::set<const browser_sync::SyncedWindowDelegate*>* delegates);

  // Find the SyncedWindowDelegate for a particular browser window id. Note
  // that this only considers Browser windows (AppWindow are not considered).
  static const browser_sync::SyncedWindowDelegate* FindByIdHelper(
      SessionID::id_type id);

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID::id_type GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsApp() const override;
  bool IsTypeTabbed() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const browser_sync::SyncedTabDelegate* tab) const override;
  browser_sync::SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID::id_type GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSyncedWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
