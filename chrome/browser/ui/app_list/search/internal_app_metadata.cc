// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/internal_app_metadata.h"

#include "base/no_destructor.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"

namespace app_list {

const std::vector<InternalApp>& GetInternalAppList() {
  static const base::NoDestructor<std::vector<InternalApp>> internal_app_list(
      {{kInternalAppIdKeyboardShortcutViewer,
        IDS_LAUNCHER_SEARCHABLE_APP_KEYBOARD_SHORTCUT_VIEWER,
        IDR_KEYBOARD_SHORTCUT_VIEWER_LOGO_192}});
  return *internal_app_list;
}

int GetIconResourceIdByAppId(const std::string& app_id) {
  for (const auto& app : GetInternalAppList()) {
    if (app_id == app.app_id)
      return app.icon_resource_id;
  }
  return 0;
}

}  // namespace app_list
