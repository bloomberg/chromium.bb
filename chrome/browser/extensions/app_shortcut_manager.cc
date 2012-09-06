// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_shortcut_manager.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
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

namespace {
// Allow tests to disable shortcut creation, to prevent developers' desktops
// becoming overrun with shortcuts.
bool disable_shortcut_creation_for_tests = false;

#if defined(OS_MACOSX)
const int kDesiredSizes[] = {16, 32, 128, 256, 512};
#else
const int kDesiredSizes[] = {32};
#endif

ShellIntegration::ShortcutInfo ShortcutInfoForExtensionAndProfile(
    const Extension* extension, Profile* profile) {
  ShellIntegration::ShortcutInfo shortcut_info;
  shortcut_info.extension_id = extension->id();
  shortcut_info.url = GURL(extension->launch_web_url());
  shortcut_info.title = UTF8ToUTF16(extension->name());
  shortcut_info.description = UTF8ToUTF16(extension->description());
  shortcut_info.extension_path = extension->path();
  shortcut_info.create_in_applications_menu = true;
  shortcut_info.create_in_quick_launch_bar = true;
  shortcut_info.create_on_desktop = true;
  shortcut_info.profile_path = profile->GetPath();
  return shortcut_info;
}

}  // namespace

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile),
      tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

void AppShortcutManager::OnImageLoaded(const gfx::Image& image,
                                       const std::string& extension_id,
                                       int index) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image.IsEmpty()) {
    gfx::Image default_icon =
        ResourceBundle::GetSharedInstance().GetImageNamed(IDR_APP_DEFAULT_ICON);
    int size = kDesiredSizes[arraysize(kDesiredSizes) - 1];
    SkBitmap bmp = skia::ImageOperations::Resize(
          *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
          size, size);
    shortcut_info_.favicon = gfx::Image(bmp);
  } else {
    shortcut_info_.favicon = image;
  }

  web_app::CreateShortcuts(shortcut_info_);
}

void AppShortcutManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
#if !defined(OS_MACOSX)
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      if (!disable_shortcut_creation_for_tests &&
          extension->is_platform_app() &&
          extension->location() != Extension::LOAD) {
        InstallApplicationShortcuts(extension);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      if (!disable_shortcut_creation_for_tests)
        DeleteApplicationShortcuts(extension);
      break;
    }
    default:
      NOTREACHED();
  }
#endif
}

// static
void AppShortcutManager::SetShortcutCreationDisabledForTesting(bool disabled) {
  disable_shortcut_creation_for_tests = disabled;
}

void AppShortcutManager::InstallApplicationShortcuts(
    const Extension* extension) {
  shortcut_info_ = ShortcutInfoForExtensionAndProfile(extension, profile_);

  std::vector<ImageLoadingTracker::ImageRepresentation> info_list;
  for (size_t i = 0; i < arraysize(kDesiredSizes); ++i) {
    int size = kDesiredSizes[i];
    ExtensionResource resource = extension->GetIconResource(
        size, ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(ImageLoadingTracker::ImageRepresentation(
          resource,
          ImageLoadingTracker::ImageRepresentation::RESIZE_WHEN_LARGER,
          gfx::Size(size, size),
          ui::SCALE_FACTOR_100P));
    }
  }

  if (info_list.empty()) {
    size_t i = arraysize(kDesiredSizes) - 1;
    int size = kDesiredSizes[i];

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is prefered to making a small icon larger, so
    // look for a larger icon first:
    ExtensionResource resource = extension->GetIconResource(
        size, ExtensionIconSet::MATCH_BIGGER);
    if (resource.empty()) {
      resource = extension->GetIconResource(
          size, ExtensionIconSet::MATCH_SMALLER);
    }
    info_list.push_back(ImageLoadingTracker::ImageRepresentation(
        resource,
        ImageLoadingTracker::ImageRepresentation::RESIZE_WHEN_LARGER,
        gfx::Size(size, size),
        ui::SCALE_FACTOR_100P));
  }

  // |icon_resources| may still be empty at this point, in which case LoadImage
  // will call the OnImageLoaded callback with an empty image and exit
  // immediately.
  tracker_.LoadImages(extension, info_list, ImageLoadingTracker::DONT_CACHE);
}

void AppShortcutManager::DeleteApplicationShortcuts(
    const Extension* extension) {
  ShellIntegration::ShortcutInfo delete_info =
      ShortcutInfoForExtensionAndProfile(extension, profile_);
  web_app::DeleteAllShortcuts(delete_info);
}

}  // namespace extensions
