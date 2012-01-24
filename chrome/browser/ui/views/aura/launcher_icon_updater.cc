// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/aura/launcher_app_icon_loader.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

// static
LauncherIconUpdater::Instances* LauncherIconUpdater::instances_ = NULL;

LauncherIconUpdater::AppTabDetails::AppTabDetails() : id(0) {
}

LauncherIconUpdater::AppTabDetails::~AppTabDetails() {
}

LauncherIconUpdater::LauncherIconUpdater(
    aura::Window* window,
    TabStripModel* tab_model,
    ash::LauncherModel* launcher_model,
    Type type)
    : window_(window),
      tab_model_(tab_model),
      launcher_model_(launcher_model),
      type_(type),
      item_id_(-1) {
  if (!instances_)
    instances_ = new Instances;
  instances_->push_back(this);
  app_icon_loader_.reset(
      new LauncherAppIconLoader(tab_model_->profile(), this));
}

LauncherIconUpdater::~LauncherIconUpdater() {
  Instances::iterator instance_iterator =
      std::find(instances_->begin(), instances_->end(), this);
  DCHECK(instance_iterator != instances_->end());
  instances_->erase(instance_iterator);
  tab_model_->RemoveObserver(this);
  if (item_id_ != -1)
    RemoveItemByID(item_id_);
  for (AppTabMap::iterator i = app_map_.begin(); i != app_map_.end(); ++i)
    RemoveItemByID(i->second.id);
}

void LauncherIconUpdater::Init() {
  tab_model_->AddObserver(this);
  if (type_ == TYPE_APP) {
    // App type never changes, create the launcher item immediately.
    item_id_ = launcher_model_->next_id();
    ash::LauncherItem item(ash::TYPE_APP);
    launcher_model_->Add(launcher_model_->item_count(), item);
  } else {
    // Determine if we have any tabs that should get launcher items.
    std::vector<TabContentsWrapper*> app_tabs;
    for (int i = 0; i < tab_model_->count(); ++i) {
      TabContentsWrapper* tab = tab_model_->GetTabContentsAt(i);
      if (!app_icon_loader_->GetAppID(tab).empty())
        app_tabs.push_back(tab);
    }

    if (static_cast<int>(app_map_.size()) != tab_model_->count())
      CreateTabbedItem();

    // Create items for the app tabs.
    for (size_t i = 0; i < app_tabs.size(); ++i)
      AddAppItem(app_tabs[i]);
  }
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

// static
LauncherIconUpdater* LauncherIconUpdater::Create(Browser* browser) {
  Type type;
  if (browser->type() == Browser::TYPE_TABBED)
    type = TYPE_TABBED;
  else if (browser->is_app())
    type = TYPE_APP;
  else
    return NULL;
  ash::LauncherModel* model = ash::Shell::GetInstance()->launcher()->model();
  LauncherIconUpdater* icon_updater =
      new LauncherIconUpdater(browser->window()->GetNativeHandle(),
                              browser->tabstrip_model(), model, type);
  icon_updater->Init();
  return icon_updater;
}

// static
void LauncherIconUpdater::ActivateByID(ash::LauncherID id) {
  TabContentsWrapper* tab = NULL;
  const LauncherIconUpdater* updater = GetLauncherByID(id, &tab);
  if (!updater) {
    NOTREACHED();
    return;
  }
  ash::ActivateWindow(updater->window_);
  if (tab) {
    updater->tab_model_->ActivateTabAt(
        updater->tab_model_->GetIndexOfTabContents(tab), true);
  }
}

// static
string16 LauncherIconUpdater::GetTitleByID(ash::LauncherID id) {
  TabContentsWrapper* tab = NULL;
  const LauncherIconUpdater* updater = GetLauncherByID(id, &tab);
  if (!updater) {
    NOTREACHED();
    return string16();
  }
  if (tab)
    return tab->web_contents()->GetTitle();
  return updater->tab_model_->GetActiveTabContents() ?
      updater->tab_model_->GetActiveTabContents()->web_contents()->GetTitle() :
      string16();
}

// static
const LauncherIconUpdater* LauncherIconUpdater::GetLauncherByID(
    ash::LauncherID id,
    TabContentsWrapper** tab) {
  *tab = NULL;
  for (Instances::const_iterator i = instances_->begin();
       i != instances_->end(); ++i) {
    if ((*i)->ContainsID(id, tab))
      return *i;
  }
  return NULL;
}

void LauncherIconUpdater::SetAppImage(TabContentsWrapper* tab,
                                      SkBitmap* image) {
  if (app_map_.find(tab) == app_map_.end())
    return;

  int model_index = launcher_model_->ItemIndexByID(app_map_[tab].id);
  ash::LauncherItem item = launcher_model_->items()[model_index];
  item.image = image ? *image : Extension::GetDefaultIcon(true);
  launcher_model_->Set(model_index, item);
}

void LauncherIconUpdater::SetAppIconLoaderForTest(AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
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
  if (type_ == TYPE_TABBED &&
      (change_type == TabStripModelObserver::LOADING_ONLY ||
       change_type == TabStripModelObserver::ALL)) {
    UpdateAppTabState(tab, UPDATE_TAB_CHANGED);
  }

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
    int item_index = launcher_model_->ItemIndexByID(item_id_);
    if (item_index == -1)
      return;
    launcher_model_->SetPendingUpdate(item_index);
  }
}

void LauncherIconUpdater::TabInsertedAt(TabContentsWrapper* contents,
                                        int index,
                                        bool foreground) {
  if (type_ == TYPE_APP)
    return;

  UpdateAppTabState(contents, UPDATE_TAB_INSERTED);
}

void LauncherIconUpdater::TabReplacedAt(TabStripModel* tab_strip_model,
                                        TabContentsWrapper* old_contents,
                                        TabContentsWrapper* new_contents,
                                        int index) {
  app_icon_loader_->Remove(old_contents);
  AppTabMap::iterator i = app_map_.find(old_contents);
  if (i != app_map_.end()) {
    AppTabDetails details = i->second;
    app_map_.erase(i);
    i = app_map_.end();
    app_map_[new_contents] = details;
    // Refetch the image, just in case we were in the process of fetching the
    // image.
    app_icon_loader_->FetchImage(new_contents);
    UpdateAppTabState(new_contents, UPDATE_TAB_CHANGED);
  }
}

void LauncherIconUpdater::TabDetachedAt(TabContentsWrapper* contents,
                                        int index) {
  if (type_ == TYPE_APP)
    return;

  UpdateAppTabState(contents, UPDATE_TAB_REMOVED);
  if (tab_model_->count() <= 3) {
    // We can't rely on the active tab at this point as the model hasn't fully
    // adjusted itself. We can rely on the count though.
    int item_index = launcher_model_->ItemIndexByID(item_id_);
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

  int item_index = launcher_model_->ItemIndexByID(item_id_);
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

void LauncherIconUpdater::UpdateAppTabState(TabContentsWrapper* tab,
                                            UpdateType update_type) {
  bool showing_app_item = app_map_.find(tab) != app_map_.end();
  std::string app_id = update_type == UPDATE_TAB_REMOVED ?
      std::string() : app_icon_loader_->GetAppID(tab);
  bool show_app = !app_id.empty();
  if (showing_app_item == show_app) {
    if (!show_app) {
      if (item_id_ == -1 && update_type == UPDATE_TAB_INSERTED) {
        // A new non-app tab was added and we have no app tabs. Add one now.
        CreateTabbedItem();
      } else if (item_id_ != -1 && update_type == UPDATE_TAB_REMOVED &&
                 tab_model_->count() == (static_cast<int>(app_map_.size()))) {
        RemoveItemByID(item_id_);
        item_id_ = -1;
      }
      return;
    }

    if (app_id != app_map_[tab].app_id) {
      // The extension changed.
      app_map_[tab].app_id = app_id;
      app_icon_loader_->FetchImage(tab);
    }
    return;
  }

  if (showing_app_item) {
    // Going from showing to not showing.
    ash::LauncherID launcher_id(app_map_[tab].id);
    app_map_.erase(tab);
    app_icon_loader_->Remove(tab);
    int model_index = launcher_model_->ItemIndexByID(launcher_id);
    DCHECK_NE(-1, model_index);
    if (item_id_ == -1 &&
        (update_type != UPDATE_TAB_REMOVED ||
         (tab_model_->count() != 1 &&
          tab_model_->count() == (static_cast<int>(app_map_.size()) + 1)))) {
      // Swap the item for a tabbed item.
      item_id_ = launcher_id;
      ash::LauncherItem item(ash::TYPE_TABBED);
      launcher_model_->Set(model_index, item);
    } else {
      // We have a tabbed item, so we can remove the the app item.
      launcher_model_->RemoveItemAt(model_index);
    }
  } else {
    // Going from not showing to showing.
    if (item_id_ != -1 &&
        static_cast<int>(app_map_.size()) + 1 == tab_model_->count()) {
      // All the tabs are app tabs, replace the tabbed item with the app.
      int model_index = launcher_model_->ItemIndexByID(item_id_);
      DCHECK_NE(-1, model_index);
      launcher_model_->Set(model_index, AppLauncherItem(tab, item_id_));
      app_icon_loader_->FetchImage(tab);
      item_id_ = -1;
    } else {
      AddAppItem(tab);
    }
  }
}

void LauncherIconUpdater::AddAppItem(TabContentsWrapper* tab) {
  // Newly created apps go after all existing apps. If there are no apps put it
  // at after the tabbed item, and if there is no tabbed item put it at the end.
  int index = item_id_ == -1 ? launcher_model_->item_count() :
      launcher_model_->ItemIndexByID(item_id_) + 1;
  for (AppTabMap::const_iterator i = app_map_.begin(); i != app_map_.end();
       ++i) {
    DCHECK_NE(-1, launcher_model_->ItemIndexByID(i->second.id));
    index = std::max(index, launcher_model_->ItemIndexByID(i->second.id) + 1);
  }
  launcher_model_->Add(index, AppLauncherItem(tab, -1));
  app_icon_loader_->FetchImage(tab);
}

ash::LauncherItem LauncherIconUpdater::AppLauncherItem(
    TabContentsWrapper* tab,
    ash::LauncherID id) {
  if (id == -1)
    id = launcher_model_->next_id();
  ash::LauncherItem item(ash::TYPE_APP);
  item.id = id;
  AppTabDetails details;
  details.id = item.id;
  details.app_id = app_icon_loader_->GetAppID(tab);
  app_map_[tab] = details;
  return item;
}

void LauncherIconUpdater::CreateTabbedItem() {
  DCHECK_EQ(-1, item_id_);
  item_id_ = launcher_model_->next_id();
  ash::LauncherItem item(ash::TYPE_TABBED);
  // Put the tabbed item before the app tabs. If there are no app tabs put it at
  // the end.
  int index = launcher_model_->item_count();
  for (AppTabMap::const_iterator i = app_map_.begin(); i != app_map_.end();
       ++i) {
    DCHECK_NE(-1, launcher_model_->ItemIndexByID(i->second.id));
    index = std::min(index, launcher_model_->ItemIndexByID(i->second.id));
  }
  launcher_model_->Add(index, item);
}

void LauncherIconUpdater::RemoveItemByID(ash::LauncherID id) {
  int model_index = launcher_model_->ItemIndexByID(id);
  DCHECK_NE(-1, model_index);
  launcher_model_->RemoveItemAt(model_index);
}

bool LauncherIconUpdater::ContainsID(ash::LauncherID id,
                                     TabContentsWrapper** tab) {
  if (item_id_ == id)
    return true;
  for (AppTabMap::const_iterator i = app_map_.begin(); i != app_map_.end();
       ++i) {
    if (i->second.id == id) {
      *tab = i->first;
      return true;
    }
  }
  return false;
}
