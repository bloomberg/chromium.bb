// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"

#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

// static
AppListServiceAsh* AppListServiceAsh::GetInstance() {
  return base::Singleton<AppListServiceAsh,
                         base::LeakySingletonTraits<AppListServiceAsh>>::get();
}

AppListServiceAsh::AppListServiceAsh() = default;
AppListServiceAsh::~AppListServiceAsh() = default;

void AppListServiceAsh::SetAppListControllerAndClient(
    ash::mojom::AppListController* app_list_controller,
    AppListClientImpl* app_list_client) {
  app_list_controller_ = app_list_controller;
  controller_delegate_.SetAppListController(app_list_controller);
  app_list_client_ = app_list_client;
}

ash::mojom::AppListController* AppListServiceAsh::GetAppListController() {
  return app_list_controller_;
}

app_list::SearchModel* AppListServiceAsh::GetSearchModelFromAsh() {
  DCHECK(!ash_util::IsRunningInMash());
  return ash::Shell::HasInstance()
             ? ash::Shell::Get()->app_list_controller()->search_model()
             : nullptr;
}

void AppListServiceAsh::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {}

void AppListServiceAsh::ShowAndSwitchToState(ash::AppListState state) {
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppListAndSwitchToState(state);
}

base::FilePath AppListServiceAsh::GetProfilePath(
    const base::FilePath& user_data_dir) {
  return ChromeLauncherController::instance()->profile()->GetPath();
}

void AppListServiceAsh::ShowForProfile(Profile* /*default_profile*/) {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppList();
}

void AppListServiceAsh::ShowForAppInstall(Profile* profile,
                                          const std::string& extension_id,
                                          bool start_discovery_tracking) {
  if (app_list::features::IsFullscreenAppListEnabled())
    return;
  ShowAndSwitchToState(ash::AppListState::kStateApps);
  AppListServiceImpl::ShowForAppInstall(profile, extension_id,
                                        start_discovery_tracking);
}

bool AppListServiceAsh::GetTargetVisibility() const {
  return app_list_target_visible_;
}

bool AppListServiceAsh::IsAppListVisible() const {
  return app_list_visible_;
}

void AppListServiceAsh::FlushForTesting() {
  app_list_client_->FlushMojoForTesting();
}

void AppListServiceAsh::DismissAppList() {
  if (!app_list_controller_)
    return;
  app_list_controller_->DismissAppList();
}

void AppListServiceAsh::EnableAppList(Profile* initial_profile,
                                      AppListEnableSource enable_source) {}

Profile* AppListServiceAsh::GetCurrentAppListProfile() {
  return ChromeLauncherController::instance()->profile();
}

AppListControllerDelegate* AppListServiceAsh::GetControllerDelegate() {
  return &controller_delegate_;
}

void AppListServiceAsh::CreateForProfile(Profile* default_profile) {}

void AppListServiceAsh::DestroyAppList() {
  // On Ash, the app list is torn down whenever it is dismissed, so just ensure
  // that it is dismissed.
  DismissAppList();
}

// static
AppListService* AppListService::Get() {
  return AppListServiceAsh::GetInstance();
}
