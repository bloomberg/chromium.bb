// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_shortcut_manager.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"

namespace {
  // Allow tests to disable shortcut creation, to prevent developers' desktops
  // becoming overrun with shortcuts.
  bool disable_shortcut_creation_for_tests = false;
} // namespace

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile),
      tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
}

void AppShortcutManager::OnImageLoaded(SkBitmap *image,
                                       const ExtensionResource &resource,
                                       int index) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (!image || image->isNull())
    image = ExtensionIconSource::LoadImageByResourceId(IDR_APP_DEFAULT_ICON);

  shortcut_info_.favicon = *image;
  web_app::CreateShortcut(profile_->GetPath(), shortcut_info_);
}

void AppShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_INSTALLED);
  const Extension* extension = content::Details<const Extension>(details).ptr();
  if (!disable_shortcut_creation_for_tests && extension->is_platform_app() &&
      extension->location() != Extension::LOAD)
    InstallApplicationShortcuts(extension);
}

// static
void AppShortcutManager::SetShortcutCreationDisabledForTesting(bool disabled) {
  disable_shortcut_creation_for_tests = disabled;
}

void AppShortcutManager::InstallApplicationShortcuts(
    const Extension* extension) {
#if defined(OS_MACOSX)
  // TODO(sail): For now only install shortcuts if enable platform apps is true.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePlatformApps)) {
    return;
  }
#endif

  const int kAppIconSize = 32;

  shortcut_info_.extension_id = extension->id();
  shortcut_info_.url = GURL(extension->launch_web_url());
  shortcut_info_.title = UTF8ToUTF16(extension->name());
  shortcut_info_.description = UTF8ToUTF16(extension->description());
  shortcut_info_.extension_path = extension->path();
  shortcut_info_.is_platform_app = extension->is_platform_app();
  shortcut_info_.create_in_applications_menu = true;
  shortcut_info_.create_in_quick_launch_bar = true;
  shortcut_info_.create_on_desktop = true;

  // The icon will be resized to |max_size|.
  const gfx::Size max_size(kAppIconSize, kAppIconSize);

  // Look for an icon. If there is no icon at the ideal size, we will resize
  // whatever we can get. Making a large icon smaller is prefered to making a
  // small icon larger, so look for a larger icon first:
  ExtensionResource icon_resource = extension->GetIconResource(
      kAppIconSize,
      ExtensionIconSet::MATCH_BIGGER);

  // If no icon exists that is the desired size or larger, get the
  // largest icon available:
  if (icon_resource.empty()) {
    icon_resource = extension->GetIconResource(
        kAppIconSize,
        ExtensionIconSet::MATCH_SMALLER);
  }

  // icon_resource may still be empty at this point, in which case LoadImage
  // which call the OnImageLoaded callback with a NULL image and exit
  // immediately.
  tracker_.LoadImage(extension,
                     icon_resource,
                     max_size,
                     ImageLoadingTracker::DONT_CACHE);
}
