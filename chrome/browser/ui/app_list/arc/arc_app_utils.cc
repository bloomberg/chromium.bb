// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace arc {

namespace {

// A class which handles the asynchronous ARC runtime callback to figure out if
// an app can handle a certain resolution or not.
// After LaunchAndRelease() got called, the object will destroy itself once
// done.
class LaunchAppWithoutSize {
 public:
  LaunchAppWithoutSize(content::BrowserContext* context,
                       const std::string& app_id,
                       bool landscape_mode) :
      context_(context), app_id_(app_id), landscape_mode_(landscape_mode) {}

  // This will launch the request and after the return the creator does not
  // need to delete the object anymore.
  bool LaunchAndRelease() {
    landscape_ = landscape_mode_ ?
        gfx::Rect(0, 0, NEXUS7_WIDTH, NEXUS7_HEIGHT) :
        gfx::Rect(0, 0, NEXUS5_WIDTH, NEXUS5_HEIGHT);
    if (!ash::Shell::HasInstance()) {
      // Skip this if there is no Ash shell.
      LaunchAppWithRect(context_, app_id_, landscape_);
      delete this;
      return true;
    }
    // TODO(skuhne): Change CanHandleResolution into a call which returns
    // capability flags like [PHONE/TABLET]_[LANDSCAPE/PORTRAIT] and which
    // might also return the used DP->PIX conversion constant to do better
    // size calculations.
    bool result = CanHandleResolution(context_, app_id_, landscape_,
        base::Bind(&LaunchAppWithoutSize::Callback, base::Unretained(this)));
    if (!result)
      delete this;
    return result;
  }

 private:
  // Default sizes to use.
  static const int NEXUS7_WIDTH = 960;
  static const int NEXUS7_HEIGHT = 600;
  static const int NEXUS5_WIDTH = 410;
  static const int NEXUS5_HEIGHT = 690;

  content::BrowserContext* context_;
  const std::string app_id_;
  const bool landscape_mode_;
  gfx::Rect landscape_;

  // The callback handler which gets called from the CanHandleResolution
  // function.
  void Callback(bool can_handle) {
    gfx::Size target_size = can_handle ? landscape_.size() :
        gfx::Size(NEXUS5_WIDTH, NEXUS5_HEIGHT);
    LaunchAppWithRect(context_, app_id_, getTargetRect(target_size));
    // Now that we are done, we can delete ourselves.
    delete this;
  }

  // Find a proper size and position for a given rectangle on the screen.
  // TODO(skuhne): This needs more consideration, but it is lacking
  // WindowPositioner functionality since we do not have an Aura::Window yet.
  gfx::Rect getTargetRect(const gfx::Size& size) {
    // Make sure that the window will fit into our workspace.
    // Note that Arc++ will always be on the primary screen (for now).
    // Note that Android's coordinate system is only valid inside the working
    // area. We can therefore ignore the provided left / top offsets.
    aura::Window* root = ash::Shell::GetPrimaryRootWindow();
    gfx::Rect work_area =
        display::Screen::GetScreen()->GetDisplayNearestWindow(root).work_area();

    gfx::Rect result(size);

    // We do not adjust sizes to fit the window into the work area here.
    // The reason is that the used DP<->pix transformation is different on
    // ChromeOS and ARC dependent on the device and zoom factor being used.
    // As such the real size cannot be determined here (yet). However - ARC
    // will adjust the bounds to not exceed the visible screen later.

   // ChromeOS does not give us much logic at this time, so we emulate what
    // WindowPositioner::GetDefaultWindowBounds does for now.
    // Note: Android's positioning will not overlap the shelf - as such we can
    // ignore the given workspace inset.
    // TODO(skuhne): Replace this with some more useful logic from the
    // WindowPositioner.
    result.set_x((work_area.width() - result.width()) / 2);
    result.set_y((work_area.height() - result.height()) / 2);
    return result;
  }

  DISALLOW_COPY_AND_ASSIGN(LaunchAppWithoutSize);
};

}  // namespace

bool LaunchAppWithRect(content::BrowserContext* context,
                       const std::string& app_id,
                       const gfx::Rect& target_rect) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  CHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app: " << app_id << ".";
    return false;
  }

  if (!app_info->ready) {
    VLOG(2) << "Cannot launch not-ready app: " << app_id << ".";
    return false;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << app_id << ".";
    return false;
  }
  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to launch app when bridge service is not ready: "
            << app_id << ".";
    return false;
  }

  arc::mojom::ScreenRectPtr rect = arc::mojom::ScreenRect::New();
  rect->left = target_rect.x();
  rect->right = target_rect.right();
  rect->top = target_rect.y();
  rect->bottom = target_rect.bottom();

  app_instance->LaunchApp(app_info->package_name, app_info->activity,
                          std::move(rect));
  prefs->SetLastLaunchTime(app_id, base::Time::Now());

  return true;
}

bool LaunchApp(content::BrowserContext* context, const std::string& app_id) {
  return LaunchApp(context, app_id, true);
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               bool landscape_layout) {
  return (new LaunchAppWithoutSize(context, app_id, landscape_layout))->
      LaunchAndRelease();
}


bool CanHandleResolution(content::BrowserContext* context,
    const std::string& app_id,
    const gfx::Rect& rect,
    const CanHandleResolutionCallback& callback) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot test resolution capability of unavailable app:" << app_id
            << ".";
    return false;
  }

  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  arc::mojom::AppInstance* app_instance =
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

  arc::mojom::ScreenRectPtr screen_rect = arc::mojom::ScreenRect::New();
  screen_rect->left = rect.x();
  screen_rect->right = rect.right();
  screen_rect->top = rect.y();
  screen_rect->bottom = rect.bottom();

  app_instance->CanHandleResolution(app_info->package_name, app_info->activity,
                                    std::move(screen_rect),
                                    callback);
  return true;
}

void UninstallPackage(const std::string& package_name) {
  VLOG(2) << "Uninstalling " << package_name;
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Request to uninstall package when bridge service is not ready: "
            << package_name << ".";
    return;
  }
  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to uninstall package when bridge service is not ready: "
            << package_name << ".";
    return;
  }

  if (bridge_service->app_version() < 2) {
    LOG(ERROR) << "Request to uninstall package when version "
               << bridge_service->app_version() << " does not support it";
    return;
  }
  app_instance->UninstallPackage(package_name);
}

}  // namespace arc
