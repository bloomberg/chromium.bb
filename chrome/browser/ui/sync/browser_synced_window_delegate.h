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
  virtual ~BrowserSyncedWindowDelegate();

  // SyncedWindowDelegate:
  virtual bool HasWindow() const override;
  virtual SessionID::id_type GetSessionId() const override;
  virtual int GetTabCount() const override;
  virtual int GetActiveIndex() const override;
  virtual bool IsApp() const override;
  virtual bool IsTypeTabbed() const override;
  virtual bool IsTypePopup() const override;
  virtual bool IsTabPinned(
      const browser_sync::SyncedTabDelegate* tab) const override;
  virtual browser_sync::SyncedTabDelegate* GetTabAt(int index) const override;
  virtual SessionID::id_type GetTabIdAt(int index) const override;
  virtual bool IsSessionRestoreInProgress() const override;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSyncedWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATE_H_
