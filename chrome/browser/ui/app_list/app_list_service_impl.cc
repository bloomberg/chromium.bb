// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/session_util.h"

// static
AppListServiceImpl* AppListServiceImpl::GetInstance() {
  return base::Singleton<AppListServiceImpl,
                         base::LeakySingletonTraits<AppListServiceImpl>>::get();
}

AppListServiceImpl::AppListServiceImpl()
    : local_state_(g_browser_process->local_state()), weak_factory_(this) {}

AppListServiceImpl::~AppListServiceImpl() = default;

void AppListServiceImpl::SetAppListControllerAndClient(
    ash::mojom::AppListController* app_list_controller,
    AppListClientImpl* app_list_client) {
  app_list_controller_ = app_list_controller;
  controller_delegate_.SetAppListController(app_list_controller);
  app_list_client_ = app_list_client;
  app_list_client_->set_controller_delegate(&controller_delegate_);
}

ash::mojom::AppListController* AppListServiceImpl::GetAppListController() {
  return app_list_controller_;
}

app_list::SearchModel* AppListServiceImpl::GetSearchModelFromAsh() {
  DCHECK(!ash_util::IsRunningInMash());
  return ash::Shell::HasInstance()
             ? ash::Shell::Get()->app_list_controller()->search_model()
             : nullptr;
}

AppListClientImpl* AppListServiceImpl::GetAppListClient() {
  app_list_client_->UpdateProfile();
  return app_list_client_;
}

AppListControllerDelegate* AppListServiceImpl::GetControllerDelegate() {
  return &controller_delegate_;
}

Profile* AppListServiceImpl::GetCurrentAppListProfile() {
  return ChromeLauncherController::instance()->profile();
}

void AppListServiceImpl::Show() {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppList();
}

void AppListServiceImpl::ShowAndSwitchToState(ash::AppListState state) {
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppListAndSwitchToState(state);
}

void AppListServiceImpl::DismissAppList() {
  if (!app_list_controller_)
    return;
  app_list_controller_->DismissAppList();
}

bool AppListServiceImpl::GetTargetVisibility() const {
  return app_list_target_visible_;
}

bool AppListServiceImpl::IsAppListVisible() const {
  return app_list_visible_;
}

// static
AppListService* AppListService::Get() {
  return AppListServiceImpl::GetInstance();
}

void AppListServiceImpl::FlushForTesting() {
  app_list_client_->FlushMojoForTesting();
}
