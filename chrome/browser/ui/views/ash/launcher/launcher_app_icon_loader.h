// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"

class Profile;

namespace extensions {
class Extension;
}


// Default implementation of LauncherUpdater::AppIconLoader that interacts
// with the ExtensionService and ImageLoadingTracker to load images.
class LauncherAppIconLoader : public ChromeLauncherController::AppIconLoader,
                              public ImageLoadingTracker::Observer {
 public:
  LauncherAppIconLoader(Profile* profile, ChromeLauncherController* host);
  virtual ~LauncherAppIconLoader();

  // AppIconLoader:
  virtual std::string GetAppID(TabContents* tab) OVERRIDE;
  virtual bool IsValidID(const std::string& id) OVERRIDE;
  virtual void FetchImage(const std::string& id) OVERRIDE;

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

 private:
  typedef std::map<int, std::string> ImageLoaderIDToExtensionIDMap;

  // Returns the extension for the specified tab.
  const extensions::Extension* GetExtensionForTab(TabContents* tab);

  // Returns the extension by ID.
  const extensions::Extension* GetExtensionByID(const std::string& id);

  Profile* profile_;

  // ChromeLauncherController we're associated with (and owned by).
  ChromeLauncherController* host_;

  // Used to load images.
  scoped_ptr<ImageLoadingTracker> image_loader_;

  // Maps from id from the ImageLoadingTracker to the extension id.
  ImageLoaderIDToExtensionIDMap map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_APP_ICON_LOADER_H_
