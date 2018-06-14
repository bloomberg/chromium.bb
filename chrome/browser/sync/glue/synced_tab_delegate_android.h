// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

namespace content {
class WebContents;
}

class TabAndroid;

namespace browser_sync {
// On Android a tab can exist even without web contents.

// SyncedTabDelegateAndroid specializes TabContentsSyncedTabDelegate with
// support for setting web contents on a late stage (for placeholder tabs),
// when the tab is brought to memory.
class SyncedTabDelegateAndroid : public TabContentsSyncedTabDelegate {
 public:
  explicit SyncedTabDelegateAndroid(TabAndroid* owning_tab_);
  ~SyncedTabDelegateAndroid() override;

  // SyncedTabDelegate:
  bool IsPlaceholderTab() const override;
  int GetSyncId() const override;
  void SetSyncId(int sync_id) override;

  // Set the web contents for this tab and handles source tab ID initialization.
  void SetWebContents(content::WebContents* web_contents,
                      content::WebContents* source_web_contents);
  // Set web contents to null.
  void ResetWebContents();

 private:
  TabAndroid* tab_android_;

  DISALLOW_COPY_AND_ASSIGN(SyncedTabDelegateAndroid);
};
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
