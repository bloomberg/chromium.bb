// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

using browser_sync::SyncedTabDelegate;

namespace browser_sync {

// static
SyncedTabDelegate* SyncedTabDelegate::ImplFromWebContents(
    content::WebContents* web_contents) {
  return TabContentsSyncedTabDelegate::FromWebContents(web_contents);
}
}  // namespace browser_sync
