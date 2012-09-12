// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_launcher_item_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

using extensions::Extension;

BrowserLauncherItemController::BrowserLauncherItemController(
    Type type,
    aura::Window* window,
    TabStripModel* tab_model,
    ChromeLauncherController* launcher_controller,
    const std::string& app_id)
    : LauncherItemController(type, launcher_controller),
      window_(window),
      tab_model_(tab_model),
      app_id_(app_id),
      is_incognito_(tab_model->profile()->GetOriginalProfile() !=
                    tab_model->profile() && !Profile::IsGuestSession()) {
  window_->AddObserver(this);
}

BrowserLauncherItemController::~BrowserLauncherItemController() {
  tab_model_->RemoveObserver(this);
  window_->RemoveObserver(this);
  if (launcher_id() > 0)
    launcher_controller()->CloseLauncherItem(launcher_id());
}

void BrowserLauncherItemController::Init() {
  tab_model_->AddObserver(this);
  ash::LauncherItemStatus app_status =
      ash::wm::IsActiveWindow(window_) ?
      ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
  ash::LauncherID launcher_id;
  if (type() != TYPE_TABBED) {
    launcher_id = launcher_controller()->CreateAppLauncherItem(
        this, app_id_, app_status);
  } else {
    launcher_id = launcher_controller()->CreateTabbedLauncherItem(
        this,
        is_incognito_ ? ChromeLauncherController::STATE_INCOGNITO :
                        ChromeLauncherController::STATE_NOT_INCOGNITO,
        app_status);
  }
  set_launcher_id(launcher_id);
  // In testing scenarios we can get tab strips with no active contents.
  if (tab_model_->GetActiveTabContents())
    UpdateLauncher(tab_model_->GetActiveTabContents());
}

// static
BrowserLauncherItemController* BrowserLauncherItemController::Create(
    Browser* browser) {
  // Under testing this can be called before the controller is created.
  if (!ChromeLauncherController::instance())
    return NULL;

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
  BrowserLauncherItemController* controller =
      new BrowserLauncherItemController(type,
                                        browser->window()->GetNativeWindow(),
                                        browser->tab_strip_model(),
                                        ChromeLauncherController::instance(),
                                        app_id);
  controller->Init();
  return controller;
}

void BrowserLauncherItemController::BrowserActivationStateChanged() {
  if (tab_model_->GetActiveTabContents())
    UpdateAppState(tab_model_->GetActiveTabContents());
  UpdateItemStatus();
}

string16 BrowserLauncherItemController::GetTitle() {
  if (type() == TYPE_TABBED || type() == TYPE_EXTENSION_PANEL) {
    const content::WebContents* contents =
        tab_model_->GetActiveTabContents()->web_contents();
    if (contents)
      return contents->GetTitle();
  }
  return string16();
}

bool BrowserLauncherItemController::HasWindow(aura::Window* window) const {
  return window_ == window;
}

void BrowserLauncherItemController::Open() {
  window_->Show();
  ash::wm::ActivateWindow(window_);
}

void BrowserLauncherItemController::Close() {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window_);
  if (widget)
    widget->Close();
}

void BrowserLauncherItemController::Clicked() {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(window_);
  if (widget && widget->IsActive())
    widget->Minimize();
  else
    Open();
}

void BrowserLauncherItemController::ActiveTabChanged(
    TabContents* old_contents,
    TabContents* new_contents,
    int index,
    bool user_gesture) {
  // Update immediately on a tab change.
  if (old_contents)
    UpdateAppState(old_contents);
  UpdateAppState(new_contents);
  UpdateLauncher(new_contents);
}

void BrowserLauncherItemController::TabInsertedAt(TabContents* contents,
                                                  int index,
                                                  bool foreground) {
  UpdateAppState(contents);
}

void BrowserLauncherItemController::TabDetachedAt(TabContents* contents,
                                                  int index) {
  launcher_controller()->UpdateAppState(
      contents, ChromeLauncherController::APP_STATE_REMOVED);
}

void BrowserLauncherItemController::TabChangedAt(
    TabContents* tab,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  UpdateAppState(tab);
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
    int item_index = launcher_model()->ItemIndexByID(launcher_id());
    if (item_index == -1)
      return;
    ash::LauncherItem item = launcher_model()->items()[item_index];
    item.image = SkBitmap();
    launcher_model()->Set(item_index, item);
  }
}

void BrowserLauncherItemController::TabReplacedAt(
    TabStripModel* tab_strip_model,
    TabContents* old_contents,
    TabContents* new_contents,
    int index) {
  launcher_controller()->UpdateAppState(
      old_contents, ChromeLauncherController::APP_STATE_REMOVED);
  UpdateAppState(new_contents);
}

void BrowserLauncherItemController::FaviconUpdated() {
  UpdateLauncher(tab_model_->GetActiveTabContents());
}

void BrowserLauncherItemController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == aura::client::kDrawAttentionKey)
    UpdateItemStatus();
}

void BrowserLauncherItemController::UpdateItemStatus() {
  ash::LauncherItemStatus status;
  if (ash::wm::IsActiveWindow(window_)) {
    // Clear attention state if active.
    if (window_->GetProperty(aura::client::kDrawAttentionKey))
      window_->SetProperty(aura::client::kDrawAttentionKey, false);
    status = ash::STATUS_ACTIVE;
  } else if (window_->GetProperty(aura::client::kDrawAttentionKey)) {
    status = ash::STATUS_ATTENTION;
  } else {
    status = ash::STATUS_RUNNING;
  }
  launcher_controller()->SetItemStatus(launcher_id(), status);
}

void BrowserLauncherItemController::UpdateLauncher(TabContents* tab) {
  if (type() == TYPE_APP_PANEL)
    return;  // Maintained entirely by ChromeLauncherController.

  if (!tab)
    return;  // Assume the window is going to be closed if there are no tabs.

  int item_index = launcher_model()->ItemIndexByID(launcher_id());
  if (item_index == -1)
    return;

  ash::LauncherItem item = launcher_model()->items()[item_index];
  if (type() == TYPE_EXTENSION_PANEL) {
    if (!favicon_loader_.get() ||
        favicon_loader_->web_contents() != tab->web_contents()) {
      favicon_loader_.reset(
          new LauncherFaviconLoader(this, tab->web_contents()));
    }
    // Update the icon for extension panels.
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(tab->web_contents());
    SkBitmap new_image = favicon_loader_->GetFavicon();
    if (new_image.isNull() && extensions_tab_helper->GetExtensionAppIcon())
      new_image = *extensions_tab_helper->GetExtensionAppIcon();
    // Only update the icon if we have a new image, or none has been set yet.
    // This avoids flickering to an empty image when a pinned app is opened.
    if (!new_image.isNull())
      item.image = new_image;
    else if (item.image.isNull())
      item.image = extensions::Extension::GetDefaultIcon(true);
  } else {
    DCHECK_EQ(TYPE_TABBED, type());
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (tab->favicon_tab_helper()->ShouldDisplayFavicon()) {
      item.image = tab->favicon_tab_helper()->GetFavicon().AsImageSkia();
      if (item.image.isNull()) {
        item.image = *rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
      }
    } else {
      item.image = *rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
    }
  }
  launcher_model()->Set(item_index, item);
}

void BrowserLauncherItemController::UpdateAppState(TabContents* tab) {
  ChromeLauncherController::AppState app_state;

  if (tab_model_->GetIndexOfTabContents(tab) == TabStripModel::kNoTab) {
    app_state = ChromeLauncherController::APP_STATE_REMOVED;
  } else if (tab_model_->GetActiveTabContents() == tab) {
    if (ash::wm::IsActiveWindow(window_))
      app_state = ChromeLauncherController::APP_STATE_WINDOW_ACTIVE;
    else
      app_state = ChromeLauncherController::APP_STATE_ACTIVE;
  } else {
    app_state = ChromeLauncherController::APP_STATE_INACTIVE;
  }
  launcher_controller()->UpdateAppState(tab, app_state);
}

ash::LauncherModel* BrowserLauncherItemController::launcher_model() {
  return launcher_controller()->model();
}
