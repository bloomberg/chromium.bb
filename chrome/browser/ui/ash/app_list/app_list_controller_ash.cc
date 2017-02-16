// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "extensions/common/extension.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/views/app_list_view.h"

AppListControllerDelegateAsh::AppListControllerDelegateAsh(
    app_list::AppListPresenterImpl* app_list_presenter)
    : app_list_presenter_(app_list_presenter) {}

AppListControllerDelegateAsh::~AppListControllerDelegateAsh() {}

void AppListControllerDelegateAsh::DismissView() {
  app_list_presenter_->Dismiss();
}

gfx::NativeWindow AppListControllerDelegateAsh::GetAppListWindow() {
  return app_list_presenter_->GetWindow();
}

gfx::Rect AppListControllerDelegateAsh::GetAppListBounds() {
  app_list::AppListView* app_list_view = app_list_presenter_->GetView();
  if (app_list_view)
    return app_list_view->GetBoundsInScreen();
  return gfx::Rect();
}

bool AppListControllerDelegateAsh::IsAppPinned(const std::string& app_id) {
  return ash::WmShell::Get()->shelf_delegate()->IsAppPinned(app_id);
}

bool AppListControllerDelegateAsh::IsAppOpen(const std::string& app_id) const {
  ash::ShelfID id =
      ash::WmShell::Get()->shelf_delegate()->GetShelfIDForAppID(app_id);
  return id && ChromeLauncherController::instance()->IsOpen(id);
}

void AppListControllerDelegateAsh::PinApp(const std::string& app_id) {
  ash::WmShell::Get()->shelf_delegate()->PinAppWithID(app_id);
}

void AppListControllerDelegateAsh::UnpinApp(const std::string& app_id) {
  ash::WmShell::Get()->shelf_delegate()->UnpinAppWithID(app_id);
}

AppListControllerDelegate::Pinnable AppListControllerDelegateAsh::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeLauncherController::instance()->profile());
}

void AppListControllerDelegateAsh::OnShowChildDialog() {
  app_list::AppListView* app_list_view = app_list_presenter_->GetView();
  if (app_list_view)
    app_list_view->SetAppListOverlayVisible(true);
}

void AppListControllerDelegateAsh::OnCloseChildDialog() {
  app_list::AppListView* app_list_view = app_list_presenter_->GetView();
  if (app_list_view)
    app_list_view->SetAppListOverlayVisible(false);
}

bool AppListControllerDelegateAsh::CanDoCreateShortcutsFlow() {
  return false;
}

void AppListControllerDelegateAsh::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  NOTREACHED();
}

void AppListControllerDelegateAsh::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  if (incognito)
    chrome::NewEmptyWindow(profile->GetOffTheRecordProfile());
  else
    chrome::NewEmptyWindow(profile);
}

void AppListControllerDelegateAsh::OpenURL(Profile* profile,
                                           const GURL& url,
                                           ui::PageTransition transition,
                                           WindowOpenDisposition disposition) {
  chrome::NavigateParams params(profile, url, transition);
  params.disposition = disposition;
  chrome::Navigate(&params);
}

void AppListControllerDelegateAsh::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  // Platform apps treat activations as a launch. The app can decide whether to
  // show a new window or focus an existing window as it sees fit.
  if (extension->is_platform_app()) {
    LaunchApp(profile, extension, source, event_flags);
    return;
  }

  ChromeLauncherController::instance()->ActivateApp(
      extension->id(),
      AppListSourceToLaunchSource(source),
      event_flags);

  DismissView();
}

void AppListControllerDelegateAsh::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  ChromeLauncherController::instance()->LaunchApp(
      ash::AppLauncherId(extension->id()), AppListSourceToLaunchSource(source),
      event_flags);
  DismissView();
}

void AppListControllerDelegateAsh::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  // Ash doesn't have profile switching.
  NOTREACHED();
}

bool AppListControllerDelegateAsh::ShouldShowUserIcon() {
  return false;
}

ash::ShelfLaunchSource
AppListControllerDelegateAsh::AppListSourceToLaunchSource(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return ash::LAUNCH_FROM_APP_LIST;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    default:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}
