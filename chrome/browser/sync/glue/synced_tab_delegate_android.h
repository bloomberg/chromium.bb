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
  virtual SessionID::id_type GetWindowId() const OVERRIDE;
  virtual SessionID::id_type GetSessionId() const OVERRIDE;
  virtual bool IsBeingDestroyed() const OVERRIDE;
  virtual Profile* profile() const OVERRIDE;
  virtual std::string GetExtensionAppId() const OVERRIDE;
  virtual int GetCurrentEntryIndex() const OVERRIDE;
  virtual int GetEntryCount() const OVERRIDE;
  virtual int GetPendingEntryIndex() const OVERRIDE;
  virtual content::NavigationEntry* GetPendingEntry() const OVERRIDE;
  virtual content::NavigationEntry* GetEntryAtIndex(int i) const OVERRIDE;
  virtual content::NavigationEntry* GetActiveEntry() const OVERRIDE;
  virtual bool IsPinned() const OVERRIDE;
  virtual bool HasWebContents() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual int GetSyncId() const OVERRIDE;
  virtual void SetSyncId(int sync_id) OVERRIDE;

  // Supervised user related methods.

  virtual bool ProfileIsSupervised() const OVERRIDE;
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const OVERRIDE;

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
