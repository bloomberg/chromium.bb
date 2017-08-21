// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_

#include "ash/public/cpp/shelf_types.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

namespace app_list {
class AppListPresenterImpl;
}

class AppListControllerDelegateAsh : public AppListControllerDelegate {
 public:
  explicit AppListControllerDelegateAsh(app_list::AppListPresenterImpl*);
  ~AppListControllerDelegateAsh() override;

  // AppListControllerDelegate overrides:
  void DismissView() override;
  gfx::NativeWindow GetAppListWindow() override;
  gfx::Rect GetAppInfoDialogBounds() override;
  bool IsAppPinned(const std::string& app_id) override;
  bool IsAppOpen(const std::string& app_id) const override;
  void PinApp(const std::string& app_id) override;
  void UnpinApp(const std::string& app_id) override;
  Pinnable GetPinnable(const std::string& app_id) override;
  void OnShowChildDialog() override;
  void OnCloseChildDialog() override;
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
                 int event_flags,
                 int64_t display_id) override;

 private:
  ash::ShelfLaunchSource AppListSourceToLaunchSource(AppListSource source);

  // Not owned.
  app_list::AppListPresenterImpl* app_list_presenter_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_CONTROLLER_ASH_H_
