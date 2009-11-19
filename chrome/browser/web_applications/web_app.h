// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include "base/file_path.h"
#include "base/task.h"
#include "chrome/browser/shell_integration.h"

namespace web_app {

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::wstring GenerateApplicationNameFromURL(const GURL& url);

// Callback after user dismisses CreateShortcutView. "true" indicates
// shortcut is created successfully. Otherwise, it is false.
typedef Callback1<bool>::Type CreateShortcutCallback;

// Creates a shortcut for web application based on given shortcut data.
// |root_dir| is used as root directory for persisted data such as icon.
// Directory layout is similar to what Gears has, i.e. an web application's
// file is stored under "#/host_name/scheme_port", where '#' is the
// |root_dir|.
void CreateShortcut(
    const FilePath& data_dir,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    CreateShortcutCallback* callback);

// Returns true if given url is a valid web app url.
bool IsValidUrl(const GURL& url);

};  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
