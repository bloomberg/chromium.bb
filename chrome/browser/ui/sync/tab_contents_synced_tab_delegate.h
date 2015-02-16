// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "components/sessions/session_id.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class TabContentsSyncedTabDelegate
    : public browser_sync::SyncedTabDelegate,
      public content::WebContentsUserData<TabContentsSyncedTabDelegate> {
 public:
  ~TabContentsSyncedTabDelegate() override;

  // Methods from SyncedTabDelegate.
  SessionID::id_type GetWindowId() const override;
  SessionID::id_type GetSessionId() const override;
  bool IsBeingDestroyed() const override;
  Profile* profile() const override;
  std::string GetExtensionAppId() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  int GetPendingEntryIndex() const override;
  content::NavigationEntry* GetPendingEntry() const override;
  content::NavigationEntry* GetEntryAtIndex(int i) const override;
  content::NavigationEntry* GetActiveEntry() const override;
  bool ProfileIsSupervised() const override;
  const std::vector<const content::NavigationEntry*>* GetBlockedNavigations()
      const override;
  bool IsPinned() const override;
  bool HasWebContents() const override;
  content::WebContents* GetWebContents() const override;
  int GetSyncId() const override;
  void SetSyncId(int sync_id) override;
  bool ShouldSync() const override;

 private:
  explicit TabContentsSyncedTabDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabContentsSyncedTabDelegate>;

  content::WebContents* web_contents_;
  int sync_session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsSyncedTabDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
