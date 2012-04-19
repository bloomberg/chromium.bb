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
#include "chrome/browser/profiles/profile.h"
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
                    tab_model->profile() && !Profile::IsGuestSession()),
      item_id_(-1) {
}

LauncherUpdater::~LauncherUpdater() {
  tab_model_->RemoveObserver(this);
  if (item_id_ != -1)
    launcher_delegate_->LauncherItemClosed(item_id_);
}

void LauncherUpdater::Init() {
  tab_model_->AddObserver(this);
  ash::LauncherItemStatus app_status =
      ash::wm::IsActiveWindow(window_) ?
          ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
  if (type_ != TYPE_TABBED) {
    item_id_ = launcher_delegate_->CreateAppLauncherItem(
        this, app_id_, app_status);
  } else {
    item_id_ = launcher_delegate_->CreateTabbedLauncherItem(
        this,
        is_incognito_ ? ChromeLauncherDelegate::STATE_INCOGNITO :
                        ChromeLauncherDelegate::STATE_NOT_INCOGNITO,
        app_status);
  }
  // In testing scenarios we can get tab strips with no active contents.
  if (tab_model_->GetActiveTabContents())
    UpdateLauncher(tab_model_->GetActiveTabContents());
}

// static
LauncherUpdater* LauncherUpdater::Create(Browser* browser) {
  Type type;
  std::string app_id;
  if (browser->type() == Browser::TYPE_TABBED ||
      browser->type() == Browser::TYPE_POPUP) {
    type = TYPE_TABBED;
  } else if (browser->is_app()) {
    if (browser->is_type_panel()) {
      if (browser->app_type() == Browser::APP_TYPE_CHILD)
        type = TYPE_EXTENSION_PANEL;
      else
        type = TYPE_APP_PANEL;
    } else {
      type = TYPE_TABBED;
    }
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

void LauncherUpdater::BrowserActivationStateChanged() {
  launcher_delegate_->SetItemStatus(
      item_id_,
      ash::wm::IsActiveWindow(window_) ?
          ash::STATUS_ACTIVE : ash::STATUS_RUNNING);
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
  if (index != tab_model_->active_index() ||
      !(change_type != TabStripModelObserver::LOADING_ONLY &&
        change_type != TabStripModelObserver::TITLE_NOT_LOADING)) {
    return;
  }

  if (tab->favicon_tab_helper()->FaviconIsValid() ||
      !tab->favicon_tab_helper()->ShouldDisplayFavicon()) {
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

void LauncherUpdater::FaviconUpdated() {
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

void LauncherUpdater::UpdateLauncher(TabContentsWrapper* tab) {
  if (type_ == TYPE_APP_PANEL)
    return;  // Maintained entirely by ChromeLauncherDelegate.

  if (!tab)
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model()->ItemIndexByID(item_id_);
  if (item_index == -1)
    return;

  ash::LauncherItem item = launcher_model()->items()[item_index];
  if (type_ == TYPE_EXTENSION_PANEL) {
    if (!favicon_loader_.get() ||
        favicon_loader_->web_contents() != tab->web_contents()) {
      favicon_loader_.reset(
          new LauncherFaviconLoader(this, tab->web_contents()));
    }
    // Update the icon for extension panels.
    SkBitmap new_image = favicon_loader_->GetFavicon();
    if (new_image.empty() && tab->extension_tab_helper()->GetExtensionAppIcon())
      new_image = *tab->extension_tab_helper()->GetExtensionAppIcon();
    // Only update the icon if we have a new image, or none has been set yet.
    // This avoids flickering to an empty image when a pinned app is opened.
    if (!new_image.empty())
      item.image = new_image;
    else if (item.image.empty())
      item.image = Extension::GetDefaultIcon(true);
  } else {
    DCHECK_EQ(TYPE_TABBED, type_);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (tab->favicon_tab_helper()->ShouldDisplayFavicon()) {
      item.image = tab->favicon_tab_helper()->GetFavicon();
      if (item.image.empty()) {
        item.image = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
      }
    } else {
      item.image = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    }
  }
  launcher_model()->Set(item_index, item);
}

ash::LauncherModel* LauncherUpdater::launcher_model() {
  return launcher_delegate_->model();
}
