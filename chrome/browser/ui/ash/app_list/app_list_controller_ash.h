// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/app_list/app_list_controller.h"

class AppListControllerAsh : public AppListController {
 public:
  AppListControllerAsh();
  virtual ~AppListControllerAsh();

 private:
  // AppListController overrides:
  virtual void CloseView() OVERRIDE;
  virtual bool IsAppPinned(const std::string& extension_id) OVERRIDE;
  virtual void PinApp(const std::string& extension_id) OVERRIDE;
  virtual void UnpinApp(const std::string& extension_id) OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const std::string& extension_id,
                           int event_flags) OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
