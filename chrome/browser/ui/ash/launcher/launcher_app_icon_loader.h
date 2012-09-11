// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_

#include <map>
#include <string>

#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

class Profile;

namespace extensions {
class Extension;
}


// Default implementation of LauncherUpdater::AppIconLoader that interacts
// with the ExtensionService and ImageLoadingTracker to load images.
class LauncherAppIconLoader : public ChromeLauncherController::AppIconLoader,
                              public extensions::IconImage::Observer {
 public:
  LauncherAppIconLoader(Profile* profile, ChromeLauncherController* host);
  virtual ~LauncherAppIconLoader();

  // AppIconLoader overrides:
  virtual void FetchImage(const std::string& id) OVERRIDE;
  virtual void ClearImage(const std::string& id) OVERRIDE;

  // extensions::IconImage::Observer overrides:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  typedef std::map<extensions::IconImage*, std::string> ImageToExtensionIDMap;

  Profile* profile_;

  // ChromeLauncherController we're associated with (and owned by).
  ChromeLauncherController* host_;

  // Maps from IconImage pointer to the extension id.
  ImageToExtensionIDMap map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_
