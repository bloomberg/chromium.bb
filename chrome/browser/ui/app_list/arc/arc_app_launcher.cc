// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"

#include <memory>

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

ArcAppLauncher::ArcAppLauncher(content::BrowserContext* context,
                               const std::string& app_id,
                               bool landscape_layout)
    : context_(context), app_id_(app_id), landscape_layout_(landscape_layout) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id_);
  if (app_info && app_info->ready)
    LaunchApp();
  else
    prefs->AddObserver(this);
}

ArcAppLauncher::~ArcAppLauncher() {
  if (!app_launched_) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
    if (prefs)
      prefs->RemoveObserver(this);
    VLOG(2) << "App " << app_id_ << "was not launched.";
  }
}

void ArcAppLauncher::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == app_id_ && app_info.ready)
    LaunchApp();
}

void ArcAppLauncher::OnAppReadyChanged(const std::string& app_id, bool ready) {
  if (app_id == app_id_ && ready)
    LaunchApp();
}

void ArcAppLauncher::LaunchApp() {
  DCHECK(!app_launched_);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs && prefs->GetApp(app_id_));
  prefs->RemoveObserver(this);

  if (!arc::LaunchApp(context_, app_id_, landscape_layout_))
    VLOG(2) << "Failed to launch app: " + app_id_ + ".";

  app_launched_ = true;
}
