// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

namespace {

class AppListServiceDisabled : public AppListService {
 public:
  static AppListServiceDisabled* GetInstance() {
    return Singleton<AppListServiceDisabled,
                     LeakySingletonTraits<AppListServiceDisabled> >::get();
  }

 private:
  friend struct DefaultSingletonTraits<AppListServiceDisabled>;

  AppListServiceDisabled() {}

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(void (*callback)()) OVERRIDE {}
  virtual void HandleFirstRun() OVERRIDE {}
  virtual void Init(Profile* initial_profile) OVERRIDE {}

  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) OVERRIDE {
    return base::FilePath();
  }
  virtual void SetProfilePath(const base::FilePath& profile_path) OVERRIDE {}

  virtual void Show() OVERRIDE {}
  virtual void CreateForProfile(Profile* profile) OVERRIDE {}
  virtual void ShowForProfile(Profile* profile) OVERRIDE {}
  virtual void AutoShowForProfile(Profile* profile) OVERRIDE {}
  virtual void DismissAppList() OVERRIDE {}

  virtual Profile* GetCurrentAppListProfile() OVERRIDE { return NULL; }
  virtual bool IsAppListVisible() const OVERRIDE { return false; }
  virtual void EnableAppList(Profile* initial_profile,
                             AppListEnableSource enable_source) OVERRIDE {}
  virtual AppListControllerDelegate* GetControllerDelegate() OVERRIDE {
    return NULL;
  }

  virtual void CreateShortcut() OVERRIDE {}

  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE {
    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(AppListServiceDisabled);
};

}  // namespace

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceDisabled::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {}

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {}

// static
bool AppListService::HandleLaunchCommandLine(
    const base::CommandLine& command_line,
    Profile* launch_profile) {
  return false;
}
