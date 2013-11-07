// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_CONTROLLER_DELEGATE_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_CONTROLLER_DELEGATE_LINUX_H_

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class AppListServiceLinux;
class Profile;

namespace extensions {
class Extension;
}

// Linux-specific configuration and behaviour for the AppList.
class AppListControllerDelegateLinux : public AppListControllerDelegateImpl {
 public:
  explicit AppListControllerDelegateLinux(AppListServiceLinux* service);
  virtual ~AppListControllerDelegateLinux();

  // AppListControllerDelegate overrides:
  virtual void ViewClosing() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow() OVERRIDE;

 private:
  AppListServiceLinux* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_CONTROLLER_DELEGATE_LINUX_H_
