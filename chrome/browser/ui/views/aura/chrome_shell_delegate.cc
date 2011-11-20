// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/aura/app_list_window.h"
#include "chrome/browser/ui/views/aura/status_area_host_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/launcher/launcher_types.h"

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate() {
  instance_ = this;
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

const views::View* ChromeShellDelegate::GetStatusArea() const {
  return status_area_host_->GetStatusArea();
}

// static
bool ChromeShellDelegate::ShouldCreateLauncherItemForBrowser(
    Browser* browser,
    aura_shell::LauncherItemType* type) {
  if (browser->type() == Browser::TYPE_TABBED) {
    *type = aura_shell::TYPE_TABBED;
    return true;
  }
  if (browser->is_app()) {
    *type = aura_shell::TYPE_APP;
    return true;
  }
  return false;
}

void ChromeShellDelegate::CreateNewWindow() {
  Browser* browser = Browser::Create(
      ProfileManager::GetDefaultProfile()->GetOriginalProfile());
  browser->AddSelectedTabWithURL(GURL(), content::PAGE_TRANSITION_START_PAGE);
  browser->window()->Show();
}

views::Widget* ChromeShellDelegate::CreateStatusArea() {
  status_area_host_.reset(new StatusAreaHostAura());
  views::Widget* status_area_widget =
      status_area_host_.get()->CreateStatusArea();
  return status_area_widget;
}

void ChromeShellDelegate::ShowApps() {
  AppListWindow::SetVisible(!AppListWindow::IsVisible());
}

void ChromeShellDelegate::LauncherItemClicked(
    const aura_shell::LauncherItem& item) {
  item.window->Activate();
}

bool ChromeShellDelegate::ConfigureLauncherItem(
    aura_shell::LauncherItem* item) {
  BrowserView* view = BrowserView::GetBrowserViewForNativeWindow(item->window);
  return view &&
      ShouldCreateLauncherItemForBrowser(view->browser(), &(item->type));
}
