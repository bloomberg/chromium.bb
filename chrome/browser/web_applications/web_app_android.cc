// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

namespace web_app {
namespace internals {

bool CreatePlatformShortcut(
    const FilePath& web_app_path,
    const FilePath& profile_path,
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  return true;
}

void DeletePlatformShortcuts(const FilePath& profile_path,
                             const std::string& extension_id) {}

}  // namespace internals
}  // namespace web_app
