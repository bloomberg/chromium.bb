// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_

class AppListControllerDelegate;
class Profile;

namespace extensions {
class Extension;
}

// Shows the chrome app information dialog box.
void ShowAppInfoDialog(AppListControllerDelegate* app_list_controller_delegate,
                       Profile* profile,
                       const extensions::Extension* app);

#endif  // CHROME_BROWSER_UI_APPS_APP_INFO_DIALOG_H_
