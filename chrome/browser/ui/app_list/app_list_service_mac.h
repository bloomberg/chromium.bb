// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"

@class AppListAnimationController;

namespace gfx {
class Display;
class Point;
}

namespace display {
using Display = gfx::Display;
}

// AppListServiceMac manages global resources needed for the app list to
// operate, and controls when and how the app list is opened and closed.
class AppListServiceMac : public AppListServiceImpl,
                          public apps::AppShimHandler {
 public:
  ~AppListServiceMac() override;

  // Finds the position for a window to anchor it to the dock. This chooses the
  // most appropriate position for the window based on whether the dock exists,
  // the position of the dock (calculated by the difference between the display
  // bounds and display work area), whether the mouse cursor is visible and its
  // position. Sets |target_origin| to the coordinates for the window to appear
  // at, and |start_origin| to the coordinates the window should begin animating
  // from. Coordinates are for the bottom-left coordinate of the window, in
  // AppKit space (Y positive is up).
  static void FindAnchorPoint(const gfx::Size& window_size,
                              const display::Display& display,
                              int primary_display_height,
                              bool cursor_is_visible,
                              const gfx::Point& cursor,
                              NSPoint* target_origin,
                              NSPoint* start_origin);

  void ShowWindowNearDock();
  void WindowAnimationDidEnd();
  void InitWithProfilePath(Profile* initial_profile,
                           const base::FilePath& profile_path);

  // AppListService overrides:
  void Init(Profile* initial_profile) override;
  void DismissAppList() override;
  void ShowForCustomLauncherPage(Profile* profile) override;
  void HideCustomLauncherPage() override;
  bool IsAppListVisible() const override;
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override;
  gfx::NativeWindow GetAppListWindow() override;
  void CreateShortcut() override;

    // AppShimHandler overrides:
  void OnShimLaunch(apps::AppShimHandler::Host* host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) override;
  void OnShimClose(apps::AppShimHandler::Host* host) override;
  void OnShimFocus(apps::AppShimHandler::Host* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override;
  void OnShimSetHidden(apps::AppShimHandler::Host* host, bool hidden) override;
  void OnShimQuit(apps::AppShimHandler::Host* host) override;

 protected:
  AppListServiceMac();

  // Returns the native window for the app list, or nil if can't be shown yet.
  // Note that, unlike GetAppListWindow(), this does not return null when the
  // app list is loaded, but not visible.
  virtual NSWindow* GetNativeWindow() const = 0;

  // If the app list is loaded, return true. Otherwise, if supported,
  // synchronously prepare an unpopulated app list window to begin showing on
  // screen and return true. If that's not supported, return false.
  virtual bool ReadyToShow() = 0;

 private:
  base::scoped_nsobject<AppListAnimationController> animation_controller_;
  base::scoped_nsobject<NSRunningApplication> previously_active_application_;
  NSPoint last_start_origin_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceMac);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
