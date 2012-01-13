// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"

#include "ash/launcher/launcher_model.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

LauncherIconUpdater::LauncherIconUpdater(
    TabStripModel* tab_model,
    ash::LauncherModel* launcher_model,
    aura::Window* window)
    : tab_model_(tab_model),
      launcher_model_(launcher_model),
      window_(window) {
  tab_model->AddObserver(this);
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

LauncherIconUpdater::~LauncherIconUpdater() {
  tab_model_->RemoveObserver(this);
}

void LauncherIconUpdater::ActiveTabChanged(TabContentsWrapper* old_contents,
                                           TabContentsWrapper* new_contents,
                                           int index,
                                           bool user_gesture) {
  // Update immediately on a tab change.
  UpdateLauncher(new_contents);
}

void LauncherIconUpdater::TabChangedAt(
    TabContentsWrapper* tab,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  if (index != tab_model_->active_index() ||
      !(change_type != TabStripModelObserver::LOADING_ONLY &&
        change_type != TabStripModelObserver::TITLE_NOT_LOADING)) {
    return;
  }

  if (tab->favicon_tab_helper()->FaviconIsValid()) {
    // We have the favicon, update immediately.
    UpdateLauncher(tab);
  } else {
    // Let the model know we're waiting. We delay updating as otherwise the user
    // sees flicker as we fetch the favicon.
    int item_index = launcher_model_->ItemIndexByWindow(window_);
    if (item_index == -1)
      return;
    launcher_model_->SetPendingUpdate(item_index);
  }
}

void LauncherIconUpdater::UpdateLauncher(TabContentsWrapper* tab) {
  if (!tab)
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model_->ItemIndexByWindow(window_);
  if (item_index == -1)
    return;

  if (launcher_model_->items()[item_index].type == ash::TYPE_APP) {
    // Use the app icon if we can.
    SkBitmap image;
    if (tab->extension_tab_helper()->GetExtensionAppIcon())
      image = *tab->extension_tab_helper()->GetExtensionAppIcon();
    else
      image = tab->favicon_tab_helper()->GetFavicon();
    launcher_model_->SetAppImage(item_index, image);
    return;
  }

  ash::LauncherTabbedImages images;
  if (tab->favicon_tab_helper()->ShouldDisplayFavicon()) {
    images.resize(1);
    images[0].image = tab->favicon_tab_helper()->GetFavicon();
    if (images[0].image.empty()) {
        images[0].image = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_DEFAULT_FAVICON);
    }
    images[0].user_data = tab;
  }
  launcher_model_->SetTabbedImages(item_index, images);
}
