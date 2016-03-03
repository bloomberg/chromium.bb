// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

void GetAndroidAppTargetRect(gfx::Rect& target_rect) {
  // TODO: Figure out where to put the android window.
  target_rect.SetRect(0, 0, 0, 0);
}

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

  gfx::Rect target_rect;
  GetAndroidAppTargetRect(target_rect);

  arc::ScreenRectPtr rect = arc::ScreenRect::New();
  rect->left = target_rect.x();
  rect->right = target_rect.right();
  rect->top = target_rect.y();
  rect->bottom = target_rect.bottom();

  app_instance->LaunchApp(app_info->package_name, app_info->activity,
                          std::move(rect));
  prefs->SetLastLaunchTime(app_id, base::Time::Now());

  return true;
}

bool CanHandleResolution(content::BrowserContext* context,
    const std::string& app_id,
    const gfx::Rect& rect,
    const CanHandleResolutionCallback callback) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot test resolution capability of unavailable app:" << app_id
            << ".";
    return false;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  arc::AppInstance* app_instance =
      bridge_service ? bridge_service->app_instance() : nullptr;
  if (!app_instance) {
    VLOG(2) << "Request to get resolution capability when bridge service is "
            << "not ready: " << app_id << ".";
    return false;
  }

  if (bridge_service->app_version() < 1) {
    VLOG(2)
        << "CanHandleResolution() not supported by ARC app instance version "
        << bridge_service->app_version();
    return false;
  }

  arc::ScreenRectPtr screen_rect = arc::ScreenRect::New();
  screen_rect->left = rect.x();
  screen_rect->right = rect.right();
  screen_rect->top = rect.y();
  screen_rect->bottom = rect.bottom();

  app_instance->CanHandleResolution(app_info->package_name, app_info->activity,
                                    std::move(screen_rect),
                                    callback);
  return true;
}

}  // namespace arc
