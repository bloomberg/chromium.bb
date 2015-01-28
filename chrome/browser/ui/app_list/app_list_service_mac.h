// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"

class AppListControllerDelegateImpl;

@class AppListAnimationController;
@class AppListWindowController;
template <typename T> struct DefaultSingletonTraits;

namespace gfx {
class Display;
class Point;
}

namespace test {
class AppListServiceMacTestApi;
}

// AppListServiceMac manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
class AppListServiceMac : public AppListServiceImpl,
                          public apps::AppShimHandler {
 public:
  ~AppListServiceMac() override;

  static AppListServiceMac* GetInstance();

  // Finds the position for a window to anchor it to the dock. This chooses the
  // most appropriate position for the window based on whether the dock exists,
  // the position of the dock (calculated by the difference between the display
  // bounds and display work area), whether the mouse cursor is visible and its
  // position. Sets |target_origin| to the coordinates for the window to appear
  // at, and |start_origin| to the coordinates the window should begin animating
  // from. Coordinates are for the bottom-left coordinate of the window, in
  // AppKit space (Y positive is up).
  static void FindAnchorPoint(const gfx::Size& window_size,
                              const gfx::Display& display,
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
  void ShowForProfile(Profile* requested_profile) override;
  void DismissAppList() override;
  void ShowForCustomLauncherPage(Profile* profile) override;
  bool IsAppListVisible() const override;
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override;
  gfx::NativeWindow GetAppListWindow() override;
  AppListControllerDelegate* GetControllerDelegate() override;
  Profile* GetCurrentAppListProfile() override;
  void CreateShortcut() override;

  // AppListServiceImpl overrides:
  void CreateForProfile(Profile* requested_profile) override;
  void DestroyAppList() override;

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

 private:
  friend class test::AppListServiceMacTestApi;
  friend struct DefaultSingletonTraits<AppListServiceMac>;

  AppListServiceMac();

  base::scoped_nsobject<AppListWindowController> window_controller_;
  base::scoped_nsobject<AppListAnimationController> animation_controller_;
  base::scoped_nsobject<NSRunningApplication> previously_active_application_;
  NSPoint last_start_origin_;
  Profile* profile_;
  scoped_ptr<AppListControllerDelegateImpl> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceMac);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
