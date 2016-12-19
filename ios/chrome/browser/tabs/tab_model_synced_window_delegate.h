// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"

@class TabModel;

namespace browser_sync {
class SyncedTabDelegate;
}

// A TabModelSyncedWindowDelegate is the iOS-based implementation of
// SyncedWindowDelegate.
class TabModelSyncedWindowDelegate
    : public sync_sessions::SyncedWindowDelegate {
 public:
  explicit TabModelSyncedWindowDelegate(TabModel* tab_model);
  ~TabModelSyncedWindowDelegate() override;

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID::id_type GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsApp() const override;
  bool IsTypeTabbed() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;
  // Return the tab id for the tab at |index|.
  SessionID::id_type GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  TabModel* tab_model_;  // weak, owns us.

  DISALLOW_COPY_AND_ASSIGN(TabModelSyncedWindowDelegate);
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_
