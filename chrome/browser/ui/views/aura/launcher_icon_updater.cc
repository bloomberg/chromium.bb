// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"

#include <algorithm>

#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "grit/ui_resources.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

// Max number of tabs we'll send icons over for.
const int kMaxCount = 3;

LauncherIconUpdater::LauncherIconUpdater(
    TabStripModel* tab_model,
    aura_shell::LauncherModel* launcher_model,
    aura::Window* window)
    : tab_model_(tab_model),
      launcher_model_(launcher_model),
      window_(window) {
  tab_model->AddObserver(this);
  if (tab_model->GetActiveTabContents())
    tabs_.push_front(tab_model->GetActiveTabContents());
  for (int i = 0; i < tab_model->count(); ++i) {
    if (i != tab_model->active_index())
      tabs_.push_back(tab_model->GetTabContentsAt(i));
  }
  UpdateLauncher();
}

LauncherIconUpdater::~LauncherIconUpdater() {
  tab_model_->RemoveObserver(this);
}

void LauncherIconUpdater::TabInsertedAt(TabContentsWrapper* contents,
                                        int index,
                                        bool foreground) {
  if (std::find(tabs_.begin(), tabs_.end(), contents) == tabs_.end())
    tabs_.push_back(contents);
}

void LauncherIconUpdater::TabDetachedAt(TabContentsWrapper* contents,
                                        int index) {
  Tabs::iterator i = std::find(tabs_.begin(), tabs_.end(), contents);
  bool update = i != tabs_.end() ? (i - tabs_.begin()) < kMaxCount : false;
  if (i != tabs_.end())
    tabs_.erase(i);
  if (update)
    UpdateLauncher();
}

void LauncherIconUpdater::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const TabStripSelectionModel& old_model) {
  TabContentsWrapper* tab = tab_strip_model->GetActiveTabContents();
  if (!tab)
    return;

  Tabs::iterator i = std::find(tabs_.begin(), tabs_.end(), tab);
  if (i == tabs_.begin())
    return;  // The active tab didn't change, ignore it.

  // Move the active tab to the front.
  if (i != tabs_.end())
    tabs_.erase(i);
  tabs_.push_front(tab);
  UpdateLauncher();
}

void LauncherIconUpdater::TabChangedAt(
    TabContentsWrapper* tab,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  if (change_type != TabStripModelObserver::LOADING_ONLY &&
      change_type != TabStripModelObserver::TITLE_NOT_LOADING) {
    Tabs::iterator i = std::find(tabs_.begin(), tabs_.end(), tab);
    if (i != tabs_.end() && (i - tabs_.begin()) < kMaxCount)
      UpdateLauncher();
  }
}

void LauncherIconUpdater::TabReplacedAt(TabStripModel* tab_strip_model,
                                        TabContentsWrapper* old_contents,
                                        TabContentsWrapper* new_contents,
                                        int index) {
  Tabs::iterator i = std::find(tabs_.begin(), tabs_.end(), old_contents);
  if (i != tabs_.end()) {
    int pos = i - tabs_.begin();
    tabs_[pos] = new_contents;
    if (pos < kMaxCount)
      UpdateLauncher();
  }
}

void LauncherIconUpdater::UpdateLauncher() {
  if (tabs_.empty())
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model_->ItemIndexByWindow(window_);
  if (item_index == -1)
    return;

  if (launcher_model_->items()[item_index].type == aura_shell::TYPE_APP) {
    // Use the app icon if we can.
    SkBitmap image;
    if (tabs_[0]->extension_tab_helper()->GetExtensionAppIcon())
      image = *tabs_[0]->extension_tab_helper()->GetExtensionAppIcon();
    else
      image = tabs_[0]->favicon_tab_helper()->GetFavicon();
    launcher_model_->SetAppImage(item_index, image);
    return;
  }

  aura_shell::LauncherTabbedImages images;
  size_t count = std::min(static_cast<size_t>(kMaxCount), tabs_.size());
  images.resize(count);
  for (size_t i = 0; i < count; ++i) {
    // TODO: needs to be updated for apps.
    images[i].image = tabs_[i]->favicon_tab_helper()->GetFavicon();
    if (images[i].image.empty()) {
      images[i].image = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_DEFAULT_FAVICON);
    }
    images[i].user_data = tabs_[i];
  }
  launcher_model_->SetTabbedImages(item_index, images);
}
