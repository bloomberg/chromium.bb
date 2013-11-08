// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class AppListServiceWin;
class Profile;

namespace extensions {
class Extension;
}

// Windows specific configuration and behaviour for the AppList.
class AppListControllerDelegateWin : public AppListControllerDelegateImpl {
 public:
  explicit AppListControllerDelegateWin(AppListServiceWin* service);
  virtual ~AppListControllerDelegateWin();

  // AppListControllerDelegate overrides:
  virtual bool ForceNativeDesktop() const OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow() OVERRIDE;

 private:
  AppListServiceWin* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
