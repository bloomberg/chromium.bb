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
  virtual bool ProfileIsSupervised() const override;
  virtual const std::vector<const content::NavigationEntry*>*
      GetBlockedNavigations() const override;
  virtual bool IsPinned() const override;
  virtual bool HasWebContents() const override;
  virtual content::WebContents* GetWebContents() const override;
  virtual int GetSyncId() const override;
  virtual void SetSyncId(int sync_id) override;

 private:
  explicit TabContentsSyncedTabDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabContentsSyncedTabDelegate>;

  content::WebContents* web_contents_;
  int sync_session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsSyncedTabDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_TAB_CONTENTS_SYNCED_TAB_DELEGATE_H_
