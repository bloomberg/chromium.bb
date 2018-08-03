// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_EXTERNAL_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_EXTERNAL_WEB_APPS_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"

namespace web_app {

// Scans the given directory (non-recursively) for *.json files that define
// "external web apps", the Web App analogs of "external extensions", described
// at https://developer.chrome.com/apps/external_extensions
//
// This function performs file I/O, and must not be scheduled on UI threads.
std::vector<web_app::PendingAppManager::AppInfo> ScanDirForExternalWebApps(
    base::FilePath dir);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_EXTERNAL_WEB_APPS_H_
