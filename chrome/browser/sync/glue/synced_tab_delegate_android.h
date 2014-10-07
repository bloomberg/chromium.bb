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
  virtual ~SyncedTabDelegateAndroid();

  // Methods from SyncedTabDelegate.
  virtual SessionID::id_type GetWindowId() const override;
  virtual SessionID::id_type GetSessionId() const override;
  virtual bool IsBeingDestroyed() const override;
  virtual Profile* profile() const override;
  virtual std::string GetExtensionAppId() const override;
  virtual int GetCurrentEntryIndex() const override;
  virtual int GetEntryCount() const override;
  virtual int GetPendingEntryIndex() const override;
  virtual content::NavigationEntry* GetPendingEntry() const override;
  virtual content::NavigationEntry* GetEntryAtIndex(int i) const override;
  virtual content::NavigationEntry* GetActiveEntry() const override;
  virtual bool IsPinned() const override;
  virtual bool HasWebContents() const override;
  virtual content::WebContents* GetWebContents() const override;
  virtual int GetSyncId() const override;
  virtual void SetSyncId(int sync_id) override;

  // Supervised user related methods.

  virtual bool ProfileIsSupervised() const override;
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const override;

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
