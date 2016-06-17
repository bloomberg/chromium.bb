// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_arc_app_updater.h"

LauncherArcAppUpdater::LauncherArcAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context)
    : LauncherAppUpdater(delegate, browser_context) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

LauncherArcAppUpdater::~LauncherArcAppUpdater() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  DCHECK(prefs);
  prefs->RemoveObserver(this);
}

void LauncherArcAppUpdater::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  delegate()->OnAppInstalled(browser_context(), app_id);
}

void LauncherArcAppUpdater::OnAppRemoved(const std::string& app_id) {
  delegate()->OnAppUninstalledPrepared(browser_context(), app_id);
  delegate()->OnAppUninstalled(browser_context(), app_id);
}
