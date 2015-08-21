// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

// This should be the version of ImplFromWebContents that's pulled in for non-
// android/ios builds. This is the standard content::WebContentsUserData way.
// SyncedTabDelegateAndroid is a little bit funky, and does this differently.
// Since SyncedTabDelegateAndroid implements most of its functionality by
// invoking TabContentsSyncedTabDelegate, this is the only SyncedTabDelegate
// piece that avoids being included in android builds.

namespace browser_sync {

// static
SyncedTabDelegate* SyncedTabDelegate::ImplFromWebContents(
    content::WebContents* web_contents) {
  return TabContentsSyncedTabDelegate::FromWebContents(web_contents);
}

} // namespace browser_sync
