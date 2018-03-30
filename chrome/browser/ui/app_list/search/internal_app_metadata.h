// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_METADATA_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_METADATA_H_

#include <string>
#include <vector>

namespace app_list {

constexpr char kInternalAppIdKeyboardShortcutViewer[] =
    "internal://keyboard_shortcut_viewer";

// Metadata about an internal app.
// Internal apps are these apps can run in Chrome OS directly, e.g. Keyboard
// Shortcut Viewer.
struct InternalApp {
  const char* app_id;
  int name_string_resource_id = 0;
  int icon_resource_id = 0;
};

// Returns a list of Chrome OS internal apps, which are searchable in launcher.
const std::vector<InternalApp>& GetInternalAppList();

// Returns the app's icon resource id.
// Returns 0 if |app_id| is invalid.
int GetIconResourceIdByAppId(const std::string& app_id);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_INTERNAL_APP_METADATA_H_
