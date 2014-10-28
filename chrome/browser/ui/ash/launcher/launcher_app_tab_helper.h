// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_TAB_HELPER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_TAB_HELPER_H_

#include <string>

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

class Profile;

// Default implementation of LauncherUpdater::AppTabHelper that interacts
// with ExtensionService.
class LauncherAppTabHelper : public ChromeLauncherController::AppTabHelper {
 public:
  explicit LauncherAppTabHelper(Profile* profile);
  ~LauncherAppTabHelper() override;

  // AppTabHelper:
  std::string GetAppID(content::WebContents* tab) override;
  bool IsValidIDForCurrentUser(const std::string& id) override;
  void SetCurrentUser(Profile* profile) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppTabHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_TAB_HELPER_H_
