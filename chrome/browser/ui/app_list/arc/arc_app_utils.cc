// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

bool LaunchApp(content::BrowserContext* context, const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  CHECK(prefs);

  scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app:" << app_id << ".";
    return false;
  }

  if (!app_info->ready) {
    VLOG(2) << "Cannot launch not-ready app:" << app_id << ".";
    return false;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << app_id << ".";
    return false;
  }
  arc::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << app_id << ".";
    return false;
  }

  app_instance->LaunchApp(app_info->package_name, app_info->activity);
  prefs->SetLastLaunchTime(app_id, base::Time::Now());

  return true;
}

}  // namespace arc
