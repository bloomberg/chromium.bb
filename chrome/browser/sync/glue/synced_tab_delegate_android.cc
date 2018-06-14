// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

namespace browser_sync {
SyncedTabDelegateAndroid::SyncedTabDelegateAndroid(TabAndroid* tab_android)
    : tab_android_(tab_android) {}

SyncedTabDelegateAndroid::~SyncedTabDelegateAndroid() {}

bool SyncedTabDelegateAndroid::IsPlaceholderTab() const {
  return web_contents() == nullptr;
}

int SyncedTabDelegateAndroid::GetSyncId() const {
  return tab_android_->GetSyncId();
}

void SyncedTabDelegateAndroid::SetSyncId(int sync_id) {
  tab_android_->SetSyncId(sync_id);
}

void SyncedTabDelegateAndroid::SetWebContents(
    content::WebContents* web_contents,
    content::WebContents* source_web_contents) {
  TabContentsSyncedTabDelegate::SetWebContents(web_contents);

  if (source_web_contents) {
    sync_sessions::SyncSessionsRouterTabHelper::FromWebContents(
        source_web_contents)
        ->SetSourceTabIdForChild(web_contents);
  }
}

void SyncedTabDelegateAndroid::ResetWebContents() {
  TabContentsSyncedTabDelegate::SetWebContents(nullptr);
}

}  // namespace browser_sync
