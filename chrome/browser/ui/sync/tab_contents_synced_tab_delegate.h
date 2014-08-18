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
  virtual ~TabContentsSyncedTabDelegate();

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
  virtual bool ProfileIsSupervised() const OVERRIDE;
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const OVERRIDE;
  virtual bool IsPinned() const OVERRIDE;
  virtual bool HasWebContents() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual int GetSyncId() const OVERRIDE;
  virtual void SetSyncId(int sync_id) OVERRIDE;

 private:
  explicit TabContentsSyncedTabDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabContentsSyncedTabDelegate>;

  content::WebContents* web_contents_;
  int sync_session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsSyncedTabDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
