// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_MANAGE_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_MANAGE_TAB_H_

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
}

// The Manage tab of the app info dialog, which provides insight and control
// over the app's state and usage.
class AppInfoManageTab : public AppInfoTab {
 public:
  AppInfoManageTab(gfx::NativeWindow parent_window,
                   Profile* profile,
                   const extensions::Extension* app,
                   const base::Closure& close_callback);

  virtual ~AppInfoManageTab();

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoManageTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_MANAGE_TAB_H_
