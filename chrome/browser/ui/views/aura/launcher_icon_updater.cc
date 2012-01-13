// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

void LauncherIconUpdater::TabDetachedAt(TabContentsWrapper* contents,
                                        int index) {
  if (tab_model_->count() <= 3) {
    // We can't rely on the active tab at this point as the model hasn't fully
    // adjusted itself. We can rely on the count though.
    int item_index = launcher_model_->ItemIndexByWindow(window_);
    if (item_index == -1)
      return;

    if (launcher_model_->items()[item_index].type == ash::TYPE_TABBED) {
      ash::LauncherItem new_item(launcher_model_->items()[item_index]);
      new_item.num_tabs = tab_model_->count();
      launcher_model_->Set(item_index, new_item);
    }
  }
}

void LauncherIconUpdater::UpdateLauncher(TabContentsWrapper* tab) {
  if (!tab)
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model_->ItemIndexByWindow(window_);
  if (item_index == -1)
    return;

  ash::LauncherItem item;
  if (launcher_model_->items()[item_index].type == ash::TYPE_APP) {
    // Use the app icon if we can.
    if (tab->extension_tab_helper()->GetExtensionAppIcon())
      item.image = *tab->extension_tab_helper()->GetExtensionAppIcon();
    else
      item.image = tab->favicon_tab_helper()->GetFavicon();
  } else {
    item.num_tabs = tab_model_->count();
    if (tab->favicon_tab_helper()->ShouldDisplayFavicon()) {
      item.image = tab->favicon_tab_helper()->GetFavicon();
      if (item.image.empty()) {
        item.image = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_DEFAULT_FAVICON);
      }
    }
  }
  launcher_model_->Set(item_index, item);
}
