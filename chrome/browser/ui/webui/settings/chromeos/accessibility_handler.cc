// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"

namespace chromeos {
namespace settings {

AccessibilityHandler::AccessibilityHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
}

AccessibilityHandler::~AccessibilityHandler() {}

void AccessibilityHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showChromeVoxSettings",
      base::Bind(&AccessibilityHandler::HandleShowChromeVoxSettings,
                 base::Unretained(this)));
}

void AccessibilityHandler::HandleShowChromeVoxSettings(
    const base::ListValue* args) {
  const extensions::Extension* chromevox_extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_misc::kChromeVoxExtensionId,
          extensions::ExtensionRegistry::ENABLED);
  if (!chromevox_extension)
    return;
  extensions::ExtensionTabUtil::OpenOptionsPage(
      chromevox_extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

}  // namespace settings
}  // namespace chromeos
