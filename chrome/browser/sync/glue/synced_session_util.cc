// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session_util.h"

#include "chrome/common/url_constants.h"
#include "components/sessions/core/session_types.h"
#include "content/public/common/url_constants.h"

namespace browser_sync {

// Note: chrome:// and file:// are not considered valid urls (for syncing).
bool ShouldSyncURL(const GURL& url) {
  return url.is_valid() && !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(chrome::kChromeNativeScheme) && !url.SchemeIsFile();
}

bool ShouldSyncSessionTab(const sessions::SessionTab& tab) {
  for (auto&& navigation : tab.navigations) {
    if (ShouldSyncURL(navigation.virtual_url())) {
      return true;
    }
  }
  return false;
}

bool ShouldSyncSessionWindow(const sessions::SessionWindow& window) {
  for (auto* tab : window.tabs) {
    if (ShouldSyncSessionTab(*tab)) {
      return true;
    }
  }
  return false;
}

}  // namespace browser_sync
