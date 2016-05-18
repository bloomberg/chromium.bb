// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_disabled_mac.h"

#include "base/compiler_specific.h"
#import "base/mac/foundation_util.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/common/mac/app_mode_common.h"

namespace {

// Stub handler for the old app_list shim that opens chrome://apps when a copy
// of the App Launcher .app bundle created with Chrome < m52 tries to connect.
class AppsPageShimHandler : public apps::AppShimHandler {
 public:
  explicit AppsPageShimHandler(void (*action)(Profile*)) : action_(action) {}

 private:
  // AppShimHandler:
  void OnShimLaunch(apps::AppShimHandler::Host* host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) override {
    AppController* controller =
        base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
    action_([controller lastProfile]);

    // Always close the shim process immediately.
    host->OnAppLaunchComplete(apps::APP_SHIM_LAUNCH_DUPLICATE_HOST);
  }
  void OnShimClose(apps::AppShimHandler::Host* host) override {}
  void OnShimFocus(apps::AppShimHandler::Host* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override {}
  void OnShimSetHidden(apps::AppShimHandler::Host* host, bool hidden) override {
  }
  void OnShimQuit(apps::AppShimHandler::Host* host) override {}

  void (*action_)(Profile*);

  DISALLOW_COPY_AND_ASSIGN(AppsPageShimHandler);
};

}  // namespace

void InitAppsPageLegacyShimHandler(void (*action)(Profile*)) {
  static AppsPageShimHandler* handler = nullptr;
  if (!handler) {
    handler = new AppsPageShimHandler(action);
    apps::AppShimHandler::RegisterHandler(app_mode::kAppListModeId, handler);
  }
}
