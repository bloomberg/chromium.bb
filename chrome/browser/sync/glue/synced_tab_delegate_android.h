// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class TabAndroid;
namespace browser_sync {
// On Android a tab can exist even without web contents.

// SyncedTabDelegateAndroid wraps TabContentsSyncedTabDelegate and provides
// a method to set web contents later when tab is brought to memory.
class SyncedTabDelegateAndroid : public browser_sync::SyncedTabDelegate {
 public:
  explicit SyncedTabDelegateAndroid(TabAndroid* owning_tab_);
  ~SyncedTabDelegateAndroid() override;

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
  bool IsPinned() const override;
  bool HasWebContents() const override;
  content::WebContents* GetWebContents() const override;
  int GetSyncId() const override;
  void SetSyncId(int sync_id) override;
  bool ShouldSync() const override;

  // Supervised user related methods.

  bool ProfileIsSupervised() const override;
  const std::vector<const content::NavigationEntry*>* GetBlockedNavigations()
      const override;

  // Set the web contents for this tab. Also creates
  // TabContentsSyncedTabDelegate for this tab.
  virtual void SetWebContents(content::WebContents* web_contents);
  // Set web contents to null.
  virtual void ResetWebContents();

 private:
  content::WebContents* web_contents_;
  TabAndroid* tab_android_;

  DISALLOW_COPY_AND_ASSIGN(SyncedTabDelegateAndroid);
};
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
