// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

#include "components/sync_sessions/tab_node_pool.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BrowserSyncedTabDelegate);

BrowserSyncedTabDelegate::BrowserSyncedTabDelegate(
    content::WebContents* web_contents)
    : sync_id_(sync_sessions::TabNodePool::kInvalidTabNodeID) {
  SetWebContents(web_contents);
}

BrowserSyncedTabDelegate::~BrowserSyncedTabDelegate() {}

bool BrowserSyncedTabDelegate::IsPlaceholderTab() const {
  return false;
}

int BrowserSyncedTabDelegate::GetSyncId() const {
  return sync_id_;
}

void BrowserSyncedTabDelegate::SetSyncId(int sync_id) {
  sync_id_ = sync_id;
}
