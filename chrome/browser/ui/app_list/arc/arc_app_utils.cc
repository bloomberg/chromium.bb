// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include <string>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace arc {

namespace {

// Default sizes to use.
constexpr int kNexus7Width = 960;
constexpr int kNexus7Height = 600;
constexpr int kNexus5Width = 410;
constexpr int kNexus5Height = 690;

// Minimum required versions.
constexpr int kMinVersion = 0;
constexpr int kCanHandleResolutionMinVersion = 1;
constexpr int kUninstallPackageMinVersion = 2;
constexpr int kShowPackageInfoMinVersion = 5;
constexpr int kRemoveIconMinVersion = 9;
constexpr int kShowPackageInfoOnPageMinVersion = 10;

// Service name strings.
constexpr char kCanHandleResolutionStr[] = "get resolution capability";
constexpr char kLaunchAppStr[] = "launch app";
constexpr char kShowPackageInfoStr[] = "show package info";
constexpr char kUninstallPackageStr[] = "uninstall package";
constexpr char kRemoveIconStr[] = "remove icon";

// Helper function which returns the AppInstance. Create related logs when error
// happens.
arc::mojom::AppInstance* GetAppInstance(int required_version,
                                        const std::string& service_name) {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "Request to " << service_name
            << " when bridge service is not ready.";
    return nullptr;
  }

  arc::mojom::AppInstance* app_instance = bridge_service->app_instance();
  if (!app_instance) {
    VLOG(2) << "Request to " << service_name
            << " when mojom::app_instance is not ready.";
    return nullptr;
  }

  int bridge_version = bridge_service->app_version();
  if (bridge_version < required_version) {
    VLOG(2) << "Request to " << service_name << " when Arc version "
            << bridge_version << " does not support it.";
    return nullptr;
  }

  return app_instance;
}

// Find a proper size and position for a given rectangle on the screen.
// TODO(skuhne): This needs more consideration, but it is lacking
// WindowPositioner functionality since we do not have an Aura::Window yet.
gfx::Rect GetTargetRect(const gfx::Size& size) {
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

// A class which handles the asynchronous ARC runtime callback to figure out if
// an app can handle a certain resolution or not.
// After LaunchAndRelease() got called, the object will destroy itself once
// done.
class LaunchAppWithoutSize {
 public:
  LaunchAppWithoutSize(content::BrowserContext* context,
                       const std::string& app_id,
                       bool landscape_mode) :
      context_(context),
      app_id_(app_id),
      landscape_mode_(landscape_mode) {}

  // This will launch the request and after the return the creator does not
  // need to delete the object anymore.
  bool LaunchAndRelease() {
    landscape_ = landscape_mode_ ? gfx::Rect(0, 0, kNexus7Width, kNexus7Height)
                                 : gfx::Rect(0, 0, kNexus5Width, kNexus5Height);
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
  content::BrowserContext* context_;
  const std::string app_id_;
  const bool landscape_mode_;
  gfx::Rect landscape_;

  // The callback handler which gets called from the CanHandleResolution
  // function.
  void Callback(bool can_handle) {
    gfx::Size target_size =
        can_handle ? landscape_.size() : gfx::Size(kNexus5Width, kNexus5Height);
    LaunchAppWithRect(context_, app_id_, GetTargetRect(target_size));
    // Now that we are done, we can delete ourselves.
    delete this;
  }

  DISALLOW_COPY_AND_ASSIGN(LaunchAppWithoutSize);
};

}  // namespace

const char kPlayStoreAppId[] = "gpkmicpkkebkmabiaedjognfppcchdfa";
const char kSettingsAppId[] = "mconboelelhjpkbdhhiijkgcimoangdj";

bool ShouldShowInLauncher(const std::string& app_id) {
  return (app_id != kSettingsAppId);
}

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

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kMinVersion, kLaunchAppStr);
  if (!app_instance)
    return false;

  if (app_info->shortcut) {
    app_instance->LaunchIntent(app_info->intent_uri, target_rect);
  } else {
    app_instance->LaunchApp(app_info->package_name, app_info->activity,
                            target_rect);
  }
  prefs->SetLastLaunchTime(app_id, base::Time::Now());

  return true;
}

bool LaunchAndroidSettingsApp(content::BrowserContext* context) {
  return arc::LaunchApp(context,
                        kSettingsAppId,
                        true);  // landscape_layout
}

bool LaunchApp(content::BrowserContext* context, const std::string& app_id) {
  return LaunchApp(context, app_id, true);
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               bool landscape_layout) {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (app_info && !app_info->ready) {
    if (!ash::Shell::HasInstance())
      return false;

    ChromeLauncherController* chrome_controller =
        ChromeLauncherController::instance();
    DCHECK(chrome_controller);
    chrome_controller->GetArcDeferredLauncher()->RegisterDeferredLaunch(app_id);
    return true;
  }

  return (new LaunchAppWithoutSize(context,
                                   app_id,
                                   landscape_layout))->LaunchAndRelease();
}

bool CanHandleResolution(content::BrowserContext* context,
    const std::string& app_id,
    const gfx::Rect& rect,
    const CanHandleResolutionCallback& callback) {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  DCHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot test resolution capability of unavailable app:" << app_id
            << ".";
    return false;
  }

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kCanHandleResolutionMinVersion, kCanHandleResolutionStr);
  if (!app_instance)
    return false;

  app_instance->CanHandleResolution(app_info->package_name, app_info->activity,
                                    rect, callback);
  return true;
}

void UninstallPackage(const std::string& package_name) {
  VLOG(2) << "Uninstalling " << package_name;

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kUninstallPackageMinVersion, kUninstallPackageStr);
  if (!app_instance)
    return;

  app_instance->UninstallPackage(package_name);
}

void RemoveCachedIcon(const std::string& icon_resource_id) {
  VLOG(2) << "Removing icon " << icon_resource_id;

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kRemoveIconMinVersion, kRemoveIconStr);
  if (!app_instance)
    return;

  app_instance->RemoveCachedIcon(icon_resource_id);
}

// Deprecated.
bool ShowPackageInfo(const std::string& package_name) {
  VLOG(2) << "Showing package info for " << package_name;

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kShowPackageInfoMinVersion, kShowPackageInfoStr);
  if (!app_instance)
    return false;

  app_instance->ShowPackageInfoDeprecated(
      package_name, GetTargetRect(gfx::Size(kNexus7Width, kNexus7Height)));
  return true;
}

bool ShowPackageInfoOnPage(const std::string& package_name,
                           mojom::ShowPackageInfoPage page) {
  VLOG(2) << "Showing package info for " << package_name;

  arc::mojom::AppInstance* app_instance =
      GetAppInstance(kShowPackageInfoOnPageMinVersion, kShowPackageInfoStr);
  if (!app_instance)
    return false;

  app_instance->ShowPackageInfoOnPage(
      package_name, page,
      GetTargetRect(gfx::Size(kNexus7Width, kNexus7Height)));
  return true;
}

}  // namespace arc
