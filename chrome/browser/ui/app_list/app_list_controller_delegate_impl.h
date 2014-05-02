// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

struct AppLaunchParams;
class AppListService;
class Profile;

namespace base {
class FilePath;
}

namespace extensions {
class Extension;
}

// Primary implementation of AppListControllerDelegate, used by non-Ash
// platforms.
class AppListControllerDelegateImpl : public AppListControllerDelegate {
 public:
  explicit AppListControllerDelegateImpl(AppListService* service);
  virtual ~AppListControllerDelegateImpl();

  // AppListControllerDelegate overrides:
  virtual void DismissView() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool IsAppPinned(const std::string& extension_id) OVERRIDE;
  virtual void PinApp(const std::string& extension_id) OVERRIDE;
  virtual void UnpinApp(const std::string& extension_id) OVERRIDE;
  virtual Pinnable GetPinnable() OVERRIDE;
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

 protected:
  // Perform platform-specific adjustments of |params| before OpenApplication().
  virtual void FillLaunchParams(AppLaunchParams* params);

 private:
  void OnCloseCreateShortcutsPrompt(bool created);

  AppListService* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_
