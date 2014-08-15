// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

// static
AppListServiceAsh* AppListServiceAsh::GetInstance() {
  return Singleton<AppListServiceAsh,
                   LeakySingletonTraits<AppListServiceAsh> >::get();
}

AppListServiceAsh::AppListServiceAsh()
    : controller_delegate_(new AppListControllerDelegateAsh()) {
}

AppListServiceAsh::~AppListServiceAsh() {
}

base::FilePath AppListServiceAsh::GetProfilePath(
    const base::FilePath& user_data_dir) {
  return ChromeLauncherController::instance()->profile()->GetPath();
}

void AppListServiceAsh::CreateForProfile(Profile* default_profile) {}

void AppListServiceAsh::ShowForProfile(Profile* default_profile) {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  // TODO(ananta): Handle profile changes correctly when !defined(OS_CHROMEOS).
  ash::Shell::GetInstance()->ShowAppList(NULL);
}

bool AppListServiceAsh::IsAppListVisible() const {
  return ash::Shell::GetInstance()->GetAppListTargetVisibility();
}

void AppListServiceAsh::DismissAppList() {
  ash::Shell::GetInstance()->DismissAppList();
}

void AppListServiceAsh::EnableAppList(Profile* initial_profile,
                                      AppListEnableSource enable_source) {}

gfx::NativeWindow AppListServiceAsh::GetAppListWindow() {
  if (ash::Shell::HasInstance())
    return ash::Shell::GetInstance()->GetAppListWindow();
  return NULL;
}

Profile* AppListServiceAsh::GetCurrentAppListProfile() {
  return ChromeLauncherController::instance()->profile();
}

AppListControllerDelegate* AppListServiceAsh::GetControllerDelegate() {
  return controller_delegate_.get();
}

// Windows and Linux Ash additionally supports a native UI. See
// app_list_service_{win,linux}.cc.
#if defined(OS_CHROMEOS)

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceAsh::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  AppListServiceAsh::GetInstance()->Init(initial_profile);
}

#endif  // !defined(OS_WIN)
