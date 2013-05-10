// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_ash.h"

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

namespace {

class AppListServiceAsh : public AppListService {
 public:
  static AppListServiceAsh* GetInstance() {
    return Singleton<AppListServiceAsh,
                     LeakySingletonTraits<AppListServiceAsh> >::get();
  }

 private:
  friend struct DefaultSingletonTraits<AppListServiceAsh>;

  AppListServiceAsh() {}

  // AppListService overrides:
  virtual base::FilePath GetAppListProfilePath(
      const base::FilePath& user_data_dir) OVERRIDE;
  virtual void ShowAppList(Profile* default_profile) OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual void DismissAppList() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceAsh);
};

base::FilePath AppListServiceAsh::GetAppListProfilePath(
    const base::FilePath& user_data_dir) {
  return ChromeLauncherController::instance()->profile()->GetPath();
}

void AppListServiceAsh::ShowAppList(Profile* default_profile) {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  // TODO(ananta): Handle profile changes correctly when !defined(OS_CHROMEOS).
  if (!ash::Shell::GetInstance()->GetAppListTargetVisibility())
    ash::Shell::GetInstance()->ToggleAppList(NULL);
}

bool AppListServiceAsh::IsAppListVisible() const {
  return ash::Shell::GetInstance()->GetAppListTargetVisibility();
}

void AppListServiceAsh::DismissAppList() {
  if (IsAppListVisible())
    ash::Shell::GetInstance()->ToggleAppList(NULL);
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceAsh() {
  return AppListServiceAsh::GetInstance();
}

}  // namespace chrome
