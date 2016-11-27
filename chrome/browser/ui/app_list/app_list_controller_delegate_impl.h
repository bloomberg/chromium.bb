// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

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
  ~AppListControllerDelegateImpl() override;

  // AppListControllerDelegate overrides:
  void DismissView() override;
  gfx::NativeWindow GetAppListWindow() override;
  bool IsAppPinned(const std::string& extension_id) override;
  void PinApp(const std::string& extension_id) override;
  void UnpinApp(const std::string& extension_id) override;
  Pinnable GetPinnable(const std::string& extension_id) override;
  bool IsAppOpen(const std::string& extension_id) const override;
  bool CanDoCreateShortcutsFlow() override;
  void DoCreateShortcutsFlow(Profile* profile,
                             const std::string& extension_id) override;
  void CreateNewWindow(Profile* profile, bool incognito) override;
  void OpenURL(Profile* profile,
               const GURL& url,
               ui::PageTransition transition,
               WindowOpenDisposition disposition) override;
  void ActivateApp(Profile* profile,
                   const extensions::Extension* extension,
                   AppListSource source,
                   int event_flags) override;
  void LaunchApp(Profile* profile,
                 const extensions::Extension* extension,
                 AppListSource source,
                 int event_flags) override;
  void ShowForProfileByPath(const base::FilePath& profile_path) override;
  bool ShouldShowUserIcon() override;

 private:
  void OnCloseCreateShortcutsPrompt(bool created);

  AppListService* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_IMPL_H_
