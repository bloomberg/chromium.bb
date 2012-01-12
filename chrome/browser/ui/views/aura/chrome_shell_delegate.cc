// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"

#include "ash/launcher/launcher_types.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/aura/app_list/app_list_model_builder.h"
#include "chrome/browser/ui/views/aura/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/views/aura/status_area_host_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/window.h"

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate() {
  instance_ = this;
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

StatusAreaView* ChromeShellDelegate::GetStatusArea() {
  return status_area_host_->GetStatusArea();
}

// static
bool ChromeShellDelegate::ShouldCreateLauncherItemForBrowser(
    Browser* browser,
    ash::LauncherItemType* type) {
  if (browser->type() == Browser::TYPE_TABBED) {
    *type = ash::TYPE_TABBED;
    return true;
  }
  if (browser->is_app()) {
    *type = ash::TYPE_APP;
    return true;
  }
  return false;
}

void ChromeShellDelegate::CreateNewWindow() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (browser_defaults::kAlwaysOpenIncognitoWindow &&
      IncognitoModePrefs::ShouldLaunchIncognito(
          *CommandLine::ForCurrentProcess(),
          profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  }
  Browser::OpenEmptyWindow(profile);
}

views::Widget* ChromeShellDelegate::CreateStatusArea() {
  status_area_host_.reset(new StatusAreaHostAura());
  views::Widget* status_area_widget =
      status_area_host_.get()->CreateStatusArea();
  return status_area_widget;
}

void ChromeShellDelegate::BuildAppListModel(ash::AppListModel* model) {
  AppListModelBuilder builder(ProfileManager::GetDefaultProfile(),
                              model);
  builder.Build();
}

ash::AppListViewDelegate*
ChromeShellDelegate::CreateAppListViewDelegate() {
  // Shell will own the created delegate.
  return new AppListViewDelegate;
}

std::vector<aura::Window*> ChromeShellDelegate::GetCycleWindowList() const {
  std::vector<aura::Window*> windows;
  // BrowserList maintains a list of browsers sorted by activity.
  for (BrowserList::const_reverse_iterator it =
           BrowserList::begin_last_active();
       it != BrowserList::end_last_active();
       ++it) {
    Browser* browser = *it;
    // Only cycle through tabbed browsers.
    if (browser &&
        browser->is_type_tabbed() &&
        browser->window()->GetNativeHandle())
      windows.push_back(browser->window()->GetNativeHandle());
  }
  return windows;
}

void ChromeShellDelegate::LauncherItemClicked(
    const ash::LauncherItem& item) {
  ash::ActivateWindow(item.window);
}

bool ChromeShellDelegate::ConfigureLauncherItem(
    ash::LauncherItem* item) {
  BrowserView* view = BrowserView::GetBrowserViewForNativeWindow(item->window);
  return view &&
      ShouldCreateLauncherItemForBrowser(view->browser(), &(item->type));
}
