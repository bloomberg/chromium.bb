// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class AppListServiceWin;
class Profile;

namespace extensions {
class Extension;
}

// Windows specific configuration and behaviour for the AppList.
class AppListControllerDelegateWin : public AppListControllerDelegate {
 public:
  explicit AppListControllerDelegateWin(AppListServiceWin* service);
  virtual ~AppListControllerDelegateWin();

  // AppListServiceWin overrides:
  virtual void DismissView() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual Pinnable GetPinnable() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow(bool is_platform_app) OVERRIDE;
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

 private:
  AppListServiceWin* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
