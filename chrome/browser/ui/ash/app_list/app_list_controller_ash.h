// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"

class AppListControllerDelegateAsh : public AppListControllerDelegate {
 public:
  AppListControllerDelegateAsh();
  virtual ~AppListControllerDelegateAsh();

 private:
  // AppListControllerDelegate overrides:
  virtual void DismissView() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::Rect GetAppListBounds() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool IsAppPinned(const std::string& extension_id) OVERRIDE;
  virtual void PinApp(const std::string& extension_id) OVERRIDE;
  virtual void UnpinApp(const std::string& extension_id) OVERRIDE;
  virtual Pinnable GetPinnable() OVERRIDE;
  virtual void OnShowChildDialog() OVERRIDE;
  virtual void OnCloseChildDialog() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow() OVERRIDE;
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) OVERRIDE;
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           AppListSource source,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         AppListSource source,
                         int event_flags) OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE;
  virtual bool ShouldShowUserIcon() OVERRIDE;

  ash::LaunchSource AppListSourceToLaunchSource(AppListSource source);

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
