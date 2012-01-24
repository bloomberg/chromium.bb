// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_APP_ICON_LOADER_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"

class Extension;
class Profile;

// Default implementation of LauncherIconUpdater::AppIconLoader that interacts
// with the ExtensionService and ImageLoadingTracker to load images.
class LauncherAppIconLoader : public LauncherIconUpdater::AppIconLoader,
                              public ImageLoadingTracker::Observer {
 public:
  LauncherAppIconLoader(Profile* profile, LauncherIconUpdater* icon_updater);
  virtual ~LauncherAppIconLoader();

  // AppIconLoader:
  virtual std::string GetAppID(TabContentsWrapper* tab) OVERRIDE;
  virtual void Remove(TabContentsWrapper* tab) OVERRIDE;
  virtual void FetchImage(TabContentsWrapper* tab) OVERRIDE;

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

 private:
  typedef std::map<int, TabContentsWrapper*> ImageLoaderIDToTabMap;

  // Removes |tab| from the |image_loader_id_to_tab_map_| map.
  void RemoveFromImageLoaderMap(TabContentsWrapper* tab);

  // Returns the extension for the specified tab.
  const Extension* GetExtensionForTab(TabContentsWrapper* tab);

  Profile* profile_;

  // LauncherIconUpdater we're associated with (and owned by).
  LauncherIconUpdater* icon_updater_;

  // Used to load images.
  scoped_ptr<ImageLoadingTracker> image_loader_;

  // Maps from id from the ImageLoadingTracker to the TabContentsWrapper.
  ImageLoaderIDToTabMap image_loader_id_to_tab_map_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_APP_ICON_LOADER_H_
