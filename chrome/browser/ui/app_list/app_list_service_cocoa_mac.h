// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_COCOA_MAC_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_COCOA_MAC_H_

#include "chrome/browser/ui/app_list/app_list_service_mac.h"

namespace test {
class AppListServiceMacTestApi;
}

class AppListControllerDelegateImpl;
@class AppListWindowController;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// AppListServiceCocoaMac shows and hides the Cocoa app list on Mac.
class AppListServiceCocoaMac : public AppListServiceMac {
 public:
  ~AppListServiceCocoaMac() override;

  static AppListServiceCocoaMac* GetInstance();

  // AppListService overrides:
  void ShowForProfile(Profile* requested_profile) override;
  Profile* GetCurrentAppListProfile() override;
  AppListControllerDelegate* GetControllerDelegate() override;

  // AppListServiceImpl overrides:
  void CreateForProfile(Profile* requested_profile) override;
  void DestroyAppList() override;

 protected:
  // AppListServiceMac overrides:
  NSWindow* GetNativeWindow() const override;
  bool ReadyToShow() override;

 private:
  friend class test::AppListServiceMacTestApi;
  friend struct base::DefaultSingletonTraits<AppListServiceCocoaMac>;

  AppListServiceCocoaMac();

  Profile* profile_;
  base::scoped_nsobject<AppListWindowController> window_controller_;
  scoped_ptr<AppListControllerDelegateImpl> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceCocoaMac);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_COCOA_MAC_H_
