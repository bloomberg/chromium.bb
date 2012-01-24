// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher_app_icon_loader.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/web_contents.h"

LauncherAppIconLoader::LauncherAppIconLoader(Profile* profile,
                                             LauncherIconUpdater* icon_updater)
    : profile_(profile),
      icon_updater_(icon_updater) {
}

LauncherAppIconLoader::~LauncherAppIconLoader() {
}

std::string LauncherAppIconLoader::GetAppID(TabContentsWrapper* tab) {
  const Extension* extension = GetExtensionForTab(tab);
  return extension ? extension->id() : std::string();
}

void LauncherAppIconLoader::Remove(TabContentsWrapper* tab) {
  RemoveFromImageLoaderMap(tab);
}

void LauncherAppIconLoader::FetchImage(TabContentsWrapper* tab) {
  // We may already be loading an image for this tab. Remove the entry from the
  // map so that we ignore the result when the image finishes loading.
  RemoveFromImageLoaderMap(tab);

  if (!image_loader_.get())
    image_loader_.reset(new ImageLoadingTracker(this));
  image_loader_id_to_tab_map_[image_loader_->next_id()] = tab;
  const Extension* extension = GetExtensionForTab(tab);
  image_loader_->LoadImage(
      extension,
      extension->GetIconResource(Extension::EXTENSION_ICON_SMALL,
                                 ExtensionIconSet::MATCH_BIGGER),
      gfx::Size(Extension::EXTENSION_ICON_SMALL,
                Extension::EXTENSION_ICON_SMALL),
      ImageLoadingTracker::CACHE);
}

void LauncherAppIconLoader::OnImageLoaded(SkBitmap* image,
                                          const ExtensionResource& resource,
                                          int index) {
  ImageLoaderIDToTabMap::iterator i = image_loader_id_to_tab_map_.find(index);
  if (i == image_loader_id_to_tab_map_.end())
    return;  // The tab has since been removed, do nothing.

  TabContentsWrapper* tab = i->second;
  image_loader_id_to_tab_map_.erase(i);
  icon_updater_->SetAppImage(tab, image);
}

void LauncherAppIconLoader::RemoveFromImageLoaderMap(TabContentsWrapper* tab) {
  for (ImageLoaderIDToTabMap::iterator i = image_loader_id_to_tab_map_.begin();
       i != image_loader_id_to_tab_map_.end(); ++i) {
    if (i->second == tab) {
      image_loader_id_to_tab_map_.erase(i);
      break;
    }
  }
}

const Extension* LauncherAppIconLoader::GetExtensionForTab(
    TabContentsWrapper* tab) {
  ExtensionService* extension_service = profile_->GetExtensionService();
  if (!extension_service)
    return NULL;
  return extension_service->GetInstalledApp(tab->web_contents()->GetURL());
}
