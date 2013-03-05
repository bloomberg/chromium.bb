// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_shortcut_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

AppShortcutManager::~AppShortcutManager() {}

void AppShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
#if !defined(OS_MACOSX)
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      if (extension->is_platform_app() &&
          extension->location() != Manifest::COMPONENT) {
        web_app::UpdateShortcutInfoAndIconForApp(*extension, profile_,
          base::Bind(&web_app::UpdateAllShortcuts));
      }
#endif  // !defined(OS_MACOSX)
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      DeleteApplicationShortcuts(extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppShortcutManager::DeleteApplicationShortcuts(
    const Extension* extension) {
  ShellIntegration::ShortcutInfo delete_info =
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile_);
  web_app::DeleteAllShortcuts(delete_info);
}

}  // namespace extensions
