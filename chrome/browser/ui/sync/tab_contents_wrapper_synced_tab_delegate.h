// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_WRAPPER_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_WRAPPER_SYNCED_TAB_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"

class TabContentsWrapper;

class TabContentsWrapperSyncedTabDelegate
    : public browser_sync::SyncedTabDelegate {
 public:
  explicit TabContentsWrapperSyncedTabDelegate(
      TabContentsWrapper* tab_contents_wrapper);
  virtual ~TabContentsWrapperSyncedTabDelegate();

  // Methods from SyncedTabDelegate.
  virtual SessionID::id_type GetWindowId() const OVERRIDE;
  virtual SessionID::id_type GetSessionId() const OVERRIDE;
  virtual bool IsBeingDestroyed() const OVERRIDE;
  virtual Profile* profile() const OVERRIDE;
  virtual bool HasExtensionAppId() const OVERRIDE;
  virtual const std::string& GetExtensionAppId() const OVERRIDE;
  virtual int GetCurrentEntryIndex() const OVERRIDE;
  virtual int GetEntryCount() const OVERRIDE;
  virtual int GetPendingEntryIndex() const OVERRIDE;
  virtual NavigationEntry* GetPendingEntry() const OVERRIDE;
  virtual NavigationEntry* GetEntryAtIndex(int i) const OVERRIDE;
  virtual NavigationEntry* GetActiveEntry() const OVERRIDE;

 private:
  TabContentsWrapper* tab_contents_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsWrapperSyncedTabDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_WRAPPER_SYNCED_TAB_DELEGATE_H_
