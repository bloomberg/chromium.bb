// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
#define CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_

#include <string>
#include <vector>

namespace app_list {

// Metadata about an internal app.
// Internal apps are these Chrome OS special apps, e.g. Settings, or these apps
// can run in Chrome OS directly, e.g. Keyboard Shortcut Viewer.
struct InternalApp {
  const char* app_id;

  // Name of the app.
  int name_string_resource_id = 0;

  int icon_resource_id = 0;

  // Can show as a suggested app.
  bool recommendable;

  // The string used for search query in addition to the name.
  int searchable_string_resource_id = 0;
};

// Returns a list of Chrome OS internal apps, which are searchable in launcher.
const std::vector<InternalApp>& GetInternalAppList();

// Returns the app's icon resource id.
// Returns 0 if |app_id| is invalid.
int GetIconResourceIdByAppId(const std::string& app_id);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_METADATA_H_
