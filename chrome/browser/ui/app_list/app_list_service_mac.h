// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "apps/app_shim/app_shim_handler_mac.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"

@class AppListAnimationController;
@class AppListWindowController;
template <typename T> struct DefaultSingletonTraits;

namespace test {
class AppListServiceTestApiMac;
}

// AppListServiceMac manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
class AppListServiceMac : public AppListServiceImpl,
                          public apps::AppShimHandler {
 public:
  virtual ~AppListServiceMac();

  static AppListServiceMac* GetInstance();

  void ShowWindowNearDock();

  // AppListService overrides:
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual AppListControllerDelegate* CreateControllerDelegate() OVERRIDE;
  virtual Profile* GetCurrentAppListProfile() OVERRIDE;


  // AppListServiceImpl overrides:
  virtual void CreateShortcut() OVERRIDE;

  // AppShimHandler overrides:
  virtual void OnShimLaunch(apps::AppShimHandler::Host* host,
                            apps::AppShimLaunchType launch_type,
                            const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void OnShimClose(apps::AppShimHandler::Host* host) OVERRIDE;
  virtual void OnShimFocus(apps::AppShimHandler::Host* host,
                           apps::AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void OnShimSetHidden(apps::AppShimHandler::Host* host,
                               bool hidden) OVERRIDE;
  virtual void OnShimQuit(apps::AppShimHandler::Host* host) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceMac>;
  friend class test::AppListServiceTestApiMac;

  AppListServiceMac();

  base::scoped_nsobject<AppListWindowController> window_controller_;
  base::scoped_nsobject<AppListAnimationController> animation_controller_;
  base::scoped_nsobject<NSRunningApplication> previously_active_application_;
  NSPoint last_start_origin_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceMac);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_MAC_H_
