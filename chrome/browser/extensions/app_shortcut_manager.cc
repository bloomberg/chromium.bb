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

#if defined(OS_WIN)
#include "chrome/browser/extensions/app_host_installer_win.h"
#include "chrome/installer/util/browser_distribution.h"
#endif

namespace extensions {

namespace {

#if defined(OS_MACOSX)
const int kDesiredSizes[] = {16, 32, 128, 256, 512};
#else
const int kDesiredSizes[] = {32};
#endif

ShellIntegration::ShortcutInfo ShortcutInfoForExtensionAndProfile(
    const Extension* extension, Profile* profile) {
  ShellIntegration::ShortcutInfo shortcut_info;
  web_app::UpdateShortcutInfoForApp(*extension, profile, &shortcut_info);
  shortcut_info.create_in_applications_menu = true;
  shortcut_info.create_in_quick_launch_bar = true;
  shortcut_info.create_on_desktop = true;
  return shortcut_info;
}

}  // namespace

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
#if !defined(OS_MACOSX)
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension = content::Details<const Extension>(
          details).ptr();
      if (extension->is_platform_app() &&
          extension->location() != Extension::COMPONENT) {
#if defined(OS_WIN)
        if (BrowserDistribution::GetDistribution()->AppHostIsSupported() &&
            extensions::AppHostInstaller::GetInstallWithLauncher()) {
          scoped_refptr<Extension> extension_ref(const_cast<Extension*>(
              extension));
          extensions::AppHostInstaller::EnsureAppHostInstalled(
              base::Bind(&AppShortcutManager::OnAppHostInstallationComplete,
                         weak_factory_.GetWeakPtr(), extension_ref));
        } else {
          UpdateApplicationShortcuts(extension);
        }
#else
        UpdateApplicationShortcuts(extension);
#endif
      }
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
#endif
}

#if defined(OS_WIN)
void AppShortcutManager::OnAppHostInstallationComplete(
    scoped_refptr<Extension> extension, bool app_host_install_success) {
  if (!app_host_install_success) {
    // Do not create shortcuts if App Host fails to install.
    LOG(ERROR) << "Application Runtime installation failed.";
    return;
  }
  UpdateApplicationShortcuts(extension);
}
#endif

void AppShortcutManager::UpdateApplicationShortcuts(
    const Extension* extension) {
  shortcut_info_ = ShortcutInfoForExtensionAndProfile(extension, profile_);

  std::vector<ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < arraysize(kDesiredSizes); ++i) {
    int size = kDesiredSizes[i];
    ExtensionResource resource = extension->GetIconResource(
        size, ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(ImageLoader::ImageRepresentation(
          resource,
          ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
          gfx::Size(size, size),
          ui::SCALE_FACTOR_100P));
    }
  }

  if (info_list.empty()) {
    size_t i = arraysize(kDesiredSizes) - 1;
    int size = kDesiredSizes[i];

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is preferred to making a small icon larger,
    // so look for a larger icon first:
    ExtensionResource resource = extension->GetIconResource(
        size, ExtensionIconSet::MATCH_BIGGER);
    if (resource.empty()) {
      resource = extension->GetIconResource(
          size, ExtensionIconSet::MATCH_SMALLER);
    }
    info_list.push_back(ImageLoader::ImageRepresentation(
        resource,
        ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
        gfx::Size(size, size),
        ui::SCALE_FACTOR_100P));
  }

  // |info_list| may still be empty at this point, in which case LoadImage
  // will call the OnImageLoaded callback with an empty image and exit
  // immediately.
  ImageLoader::Get(profile_)->LoadImagesAsync(extension, info_list,
      base::Bind(&AppShortcutManager::OnImageLoaded,
                 weak_factory_.GetWeakPtr()));
}

void AppShortcutManager::OnImageLoaded(const gfx::Image& image) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image.IsEmpty()) {
    gfx::Image default_icon =
        ResourceBundle::GetSharedInstance().GetImageNamed(IDR_APP_DEFAULT_ICON);
    int size = kDesiredSizes[arraysize(kDesiredSizes) - 1];
    SkBitmap bmp = skia::ImageOperations::Resize(
          *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
          size, size);
    shortcut_info_.favicon = gfx::Image::CreateFrom1xBitmap(bmp);
  } else {
    shortcut_info_.favicon = image;
  }

  web_app::UpdateAllShortcuts(shortcut_info_);
}

void AppShortcutManager::DeleteApplicationShortcuts(
    const Extension* extension) {
  ShellIntegration::ShortcutInfo delete_info =
      ShortcutInfoForExtensionAndProfile(extension, profile_);
  web_app::DeleteAllShortcuts(delete_info);
}

}  // namespace extensions
