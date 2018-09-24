// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_utils.h"

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "extensions/common/extension.h"

namespace app_list {

namespace {
constexpr char kChromeCameraAppId[] = "hfhhnacclhffhdffklopdkcgdhifgngh";
constexpr char const* kAppIdsHiddenInLauncher[] = {kChromeCameraAppId};
}  // namespace

bool ShouldShowInLauncher(const extensions::Extension* extension,
                          content::BrowserContext* context) {
  return !HideInLauncherById(extension->id()) &&
         chromeos::DemoSession::ShouldDisplayInAppLauncher(extension->id()) &&
         extensions::ui_util::ShouldDisplayInAppLauncher(extension, context);
}

bool HideInLauncherById(std::string extension_id) {
  for (auto* const id : kAppIdsHiddenInLauncher) {
    if (id == extension_id) {
      return true;
    }
  }
  return false;
}

}  // namespace app_list
