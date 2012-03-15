// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_updater.h"

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
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"

LauncherUpdater::AppTabDetails::AppTabDetails() : id(0) {
}

LauncherUpdater::AppTabDetails::~AppTabDetails() {
}

LauncherUpdater::LauncherUpdater(aura::Window* window,
                                 TabStripModel* tab_model,
                                 ChromeLauncherDelegate* delegate,
                                 Type type,
                                 const std::string& app_id)
    : window_(window),
      tab_model_(tab_model),
      launcher_delegate_(delegate),
      type_(type),
      app_id_(app_id),
      is_incognito_(tab_model->profile()->GetOriginalProfile() !=
                    tab_model->profile()),
      item_id_(-1) {
}

LauncherUpdater::~LauncherUpdater() {
  tab_model_->RemoveObserver(this);
  if (item_id_ != -1)
    launcher_delegate_->LauncherItemClosed(item_id_);
  for (AppTabMap::iterator i = app_map_.begin(); i != app_map_.end(); ++i)
    launcher_delegate_->LauncherItemClosed(i->second.id);
}

void LauncherUpdater::Init() {
  tab_model_->AddObserver(this);
  if (type_ == TYPE_PANEL) {
    favicon_loader_.reset(
        new LauncherFaviconLoader(
            this, tab_model_->GetActiveTabContents()->web_contents()));
  }
  if (type_ == TYPE_APP || type_ == TYPE_PANEL) {
    // App type never changes, create the launcher item immediately.
    ChromeLauncherDelegate::AppType app_type =
        type_ == TYPE_PANEL ? ChromeLauncherDelegate::APP_TYPE_PANEL
        : ChromeLauncherDelegate::APP_TYPE_WINDOW;
    item_id_ = launcher_delegate_->CreateAppLauncherItem(
        this, app_id_, app_type, ash::STATUS_RUNNING);
  } else {
    // Determine if we have any tabs that should get launcher items.
    std::vector<TabContentsWrapper*> app_tabs;
    for (int i = 0; i < tab_model_->count(); ++i) {
      TabContentsWrapper* tab = tab_model_->GetTabContentsAt(i);
      if (!launcher_delegate_->GetAppID(tab).empty())
        app_tabs.push_back(tab);
    }

    if (static_cast<int>(app_tabs.size()) != tab_model_->count())
      CreateTabbedItem();

    // Create items for the app tabs.
    for (size_t i = 0; i < app_tabs.size(); ++i)
      AddAppItem(app_tabs[i]);
  }
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

// static
LauncherUpdater* LauncherUpdater::Create(Browser* browser) {
  Type type;
  std::string app_id;
  if (browser->type() == Browser::TYPE_TABBED) {
    type = TYPE_TABBED;
  } else if (browser->is_app()) {
    type = browser->is_type_panel() ? TYPE_PANEL : TYPE_APP;
    app_id = web_app::GetExtensionIdFromApplicationName(browser->app_name());
  } else {
    return NULL;
  }
  LauncherUpdater* icon_updater = new LauncherUpdater(
      browser->window()->GetNativeHandle(), browser->tabstrip_model(),
      ChromeLauncherDelegate::instance(), type, app_id);
  icon_updater->Init();
  return icon_updater;
}

TabContentsWrapper* LauncherUpdater::GetTab(ash::LauncherID id) {
  for (AppTabMap::const_iterator i = app_map_.begin(); i != app_map_.end();
       ++i) {
    if (i->second.id == id)
      return i->first;
  }
  return NULL;
}

void LauncherUpdater::ActiveTabChanged(TabContentsWrapper* old_contents,
                                       TabContentsWrapper* new_contents,
                                       int index,
                                       bool user_gesture) {
  // Update immediately on a tab change.
  UpdateLauncher(new_contents);
}

void LauncherUpdater::TabChangedAt(
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
    int item_index = launcher_model()->ItemIndexByID(item_id_);
    if (item_index == -1)
      return;
    launcher_model()->SetPendingUpdate(item_index);
  }
}

void LauncherUpdater::TabInsertedAt(TabContentsWrapper* contents,
                                    int index,
                                    bool foreground) {
  if (type_ != TYPE_TABBED)
    return;

  UpdateAppTabState(contents, UPDATE_TAB_INSERTED);
}

void LauncherUpdater::TabReplacedAt(TabStripModel* tab_strip_model,
                                    TabContentsWrapper* old_contents,
                                    TabContentsWrapper* new_contents,
                                    int index) {
  if (type_ == TYPE_PANEL) {
    DCHECK(index == 0);
    if (new_contents->web_contents() != old_contents->web_contents()) {
      favicon_loader_.reset(
          new LauncherFaviconLoader(this, new_contents->web_contents()));
    }
  }

  AppTabMap::iterator i = app_map_.find(old_contents);
  if (i != app_map_.end()) {
    AppTabDetails details = i->second;
    app_map_.erase(i);
    i = app_map_.end();
    app_map_[new_contents] = details;
    UpdateAppTabState(new_contents, UPDATE_TAB_CHANGED);
  }
}

void LauncherUpdater::TabDetachedAt(TabContentsWrapper* contents, int index) {
  if (type_ != TYPE_TABBED)
    return;

  UpdateAppTabState(contents, UPDATE_TAB_REMOVED);
  if (tab_model_->count() <= 3) {
    // We can't rely on the active tab at this point as the model hasn't fully
    // adjusted itself. We can rely on the count though.
    int item_index = launcher_model()->ItemIndexByID(item_id_);
    if (item_index == -1)
      return;

    if (launcher_model()->items()[item_index].type == ash::TYPE_TABBED) {
      ash::LauncherItem new_item(launcher_model()->items()[item_index]);
      new_item.num_tabs = tab_model_->count();
      launcher_model()->Set(item_index, new_item);
    }
  }
}

void LauncherUpdater::FaviconUpdated() {
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

void LauncherUpdater::UpdateLauncher(TabContentsWrapper* tab) {
  if (type_ == TYPE_APP)
    return;  // TYPE_APP is entirely maintained by ChromeLauncherDelegate.

  if (!tab)
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model()->ItemIndexByID(item_id_);
  if (item_index == -1)
    return;

  ash::LauncherItem item = launcher_model()->items()[item_index];
  if (type_ == TYPE_PANEL) {
    // Update the icon for app panels.
    item.image = favicon_loader_->GetFavicon();
    if (item.image.empty()) {
      if (tab->extension_tab_helper()->GetExtensionAppIcon())
        item.image = *tab->extension_tab_helper()->GetExtensionAppIcon();
      else
        item.image = Extension::GetDefaultIcon(true);
    }
  } else if (launcher_model()->items()[item_index].type == ash::TYPE_APP) {
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
  launcher_model()->Set(item_index, item);
}

void LauncherUpdater::UpdateAppTabState(TabContentsWrapper* tab,
                                        UpdateType update_type) {
  bool showing_app_item = app_map_.find(tab) != app_map_.end();
  std::string app_id = update_type == UPDATE_TAB_REMOVED ?
      std::string() : launcher_delegate_->GetAppID(tab);
  bool show_app = !app_id.empty();
  if (showing_app_item == show_app) {
    if (!show_app) {
      if (item_id_ == -1 && update_type == UPDATE_TAB_INSERTED) {
        // A new non-app tab was added and we have no app tabs. Add one now.
        CreateTabbedItem();
      } else if (item_id_ != -1 && update_type == UPDATE_TAB_REMOVED &&
                 tab_model_->count() == (static_cast<int>(app_map_.size()))) {
        launcher_delegate_->LauncherItemClosed(item_id_);
        item_id_ = -1;
      }
      return;
    }

    if (app_id != app_map_[tab].app_id) {
      // The extension changed.
      app_map_[tab].app_id = app_id;
      launcher_delegate_->AppIDChanged(app_map_[tab].id, app_id);
    }
    return;
  }

  if (showing_app_item) {
    // Going from showing to not showing.
    ash::LauncherID launcher_id(app_map_[tab].id);
    app_map_.erase(tab);
    int model_index = launcher_model()->ItemIndexByID(launcher_id);
    DCHECK_NE(-1, model_index);
    if (item_id_ == -1 &&
        (update_type != UPDATE_TAB_REMOVED ||
         (tab_model_->count() != 1 &&
          tab_model_->count() == (static_cast<int>(app_map_.size()) + 1)))) {
      if (!launcher_delegate_->IsPinned(launcher_id)) {
        // Swap the item for a tabbed item.
        item_id_ = launcher_id;
        launcher_delegate_->ConvertAppToTabbed(item_id_);
      } else {
        // If the app is pinned we have to leave it and create a new tabbed
        // item.
        launcher_delegate_->LauncherItemClosed(launcher_id);
        CreateTabbedItem();
      }
      ash::LauncherItem item;
      item.type = ash::TYPE_TABBED;
      item.is_incognito = is_incognito_;
      item.num_tabs = tab_model_->count();
      launcher_model()->Set(launcher_model()->ItemIndexByID(item_id_), item);
    } else {
      // We have a tabbed item, so we can remove the the app item.
      launcher_delegate_->LauncherItemClosed(launcher_id);
    }
  } else {
    // Going from not showing to showing.
    if (item_id_ != -1 &&
        static_cast<int>(app_map_.size()) + 1 == tab_model_->count()) {
      if (launcher_delegate_->HasClosedAppItem(
              launcher_delegate_->GetAppID(tab),
              ChromeLauncherDelegate::APP_TYPE_TAB)) {
        // There's a closed item we can use. Close the tabbed item and add an
        // app item, which will end up using the closed item.
        launcher_delegate_->LauncherItemClosed(item_id_);
        AddAppItem(tab);
      } else {
        // All the tabs are app tabs, replace the tabbed item with the app.
        launcher_delegate_->ConvertTabbedToApp(
            item_id_,
            launcher_delegate_->GetAppID(tab),
            ChromeLauncherDelegate::APP_TYPE_TAB);
        RegisterAppItem(item_id_, tab);
        launcher_delegate_->SetItemStatus(item_id_, ash::STATUS_RUNNING);
      }
      item_id_ = -1;
    } else {
      AddAppItem(tab);
    }
  }
}

void LauncherUpdater::AddAppItem(TabContentsWrapper* tab) {
  ash::LauncherID id = launcher_delegate_->CreateAppLauncherItem(
      this,
      launcher_delegate_->GetAppID(tab),
      ChromeLauncherDelegate::APP_TYPE_TAB,
      ash::STATUS_RUNNING);
  RegisterAppItem(id, tab);
}

void LauncherUpdater::RegisterAppItem(ash::LauncherID id,
                                      TabContentsWrapper* tab) {
  AppTabDetails details;
  details.id = id;
  details.app_id = launcher_delegate_->GetAppID(tab);
  app_map_[tab] = details;
}

void LauncherUpdater::CreateTabbedItem() {
  DCHECK_EQ(-1, item_id_);
  item_id_ = launcher_delegate_->CreateTabbedLauncherItem(
      this,
      is_incognito_ ? ChromeLauncherDelegate::STATE_INCOGNITO :
                      ChromeLauncherDelegate::STATE_NOT_INCOGNITO);
}

bool LauncherUpdater::ContainsID(ash::LauncherID id, TabContentsWrapper** tab) {
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

ash::LauncherModel* LauncherUpdater::launcher_model() {
  return launcher_delegate_->model();
}
