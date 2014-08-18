// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "components/sessions/session_id.h"

class TabModel;

namespace browser_sync {

class SyncedTabDelegate;

class SyncedWindowDelegateAndroid : public browser_sync::SyncedWindowDelegate {
 public:
  explicit SyncedWindowDelegateAndroid(TabModel* tab_model);
  virtual ~SyncedWindowDelegateAndroid();

  // browser_sync::SyncedWindowDelegate implementation.

  virtual bool HasWindow() const OVERRIDE;
  virtual SessionID::id_type GetSessionId() const OVERRIDE;
  virtual int GetTabCount() const OVERRIDE;
  virtual int GetActiveIndex() const OVERRIDE;
  virtual bool IsApp() const OVERRIDE;
  virtual bool IsTypeTabbed() const OVERRIDE;
  virtual bool IsTypePopup() const OVERRIDE;
  virtual bool IsTabPinned(const SyncedTabDelegate* tab) const OVERRIDE;
  virtual SyncedTabDelegate* GetTabAt(int index) const OVERRIDE;
  virtual SessionID::id_type GetTabIdAt(int index) const OVERRIDE;
  virtual bool IsSessionRestoreInProgress() const OVERRIDE;

 private:
  TabModel* tab_model_;

  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegateAndroid);
};

} // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_
