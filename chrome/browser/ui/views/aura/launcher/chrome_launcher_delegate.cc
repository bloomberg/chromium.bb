// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher/chrome_launcher_delegate.h"

#include "ash/launcher/launcher_types.h"
#include "base/command_line.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"
#include "grit/theme_resources.h"

ChromeLauncherDelegate::ChromeLauncherDelegate() {
}

ChromeLauncherDelegate::~ChromeLauncherDelegate() {
}

void ChromeLauncherDelegate::CreateNewWindow() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (browser_defaults::kAlwaysOpenIncognitoWindow &&
      IncognitoModePrefs::ShouldLaunchIncognito(
          *CommandLine::ForCurrentProcess(),
          profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  }
  Browser::OpenEmptyWindow(profile);
}

void ChromeLauncherDelegate::ItemClicked(const ash::LauncherItem& item) {
  LauncherIconUpdater::ActivateByID(item.id);
}

int ChromeLauncherDelegate::GetBrowserShortcutResourceId() {
  return IDR_PRODUCT_LOGO_32;
}

string16 ChromeLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  return LauncherIconUpdater::GetTitleByID(item.id);
}
