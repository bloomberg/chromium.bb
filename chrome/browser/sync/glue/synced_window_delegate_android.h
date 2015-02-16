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
  ~SyncedWindowDelegateAndroid() override;

  // browser_sync::SyncedWindowDelegate implementation.

  bool HasWindow() const override;
  SessionID::id_type GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsApp() const override;
  bool IsTypeTabbed() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const SyncedTabDelegate* tab) const override;
  SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID::id_type GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  TabModel* tab_model_;

  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegateAndroid);
};

} // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATE_ANDROID_H_
