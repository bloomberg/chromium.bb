// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

namespace {

class AppListServiceDisabled : public AppListService {
 public:
  static AppListServiceDisabled* GetInstance() {
    return base::Singleton<
        AppListServiceDisabled,
        base::LeakySingletonTraits<AppListServiceDisabled>>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<AppListServiceDisabled>;

  AppListServiceDisabled() {}

  // AppListService overrides:
  void SetAppListNextPaintCallback(void (*callback)()) override {}
  void Init(Profile* initial_profile) override {}

  base::FilePath GetProfilePath(const base::FilePath& user_data_dir) override {
    return base::FilePath();
  }
  void SetProfilePath(const base::FilePath& profile_path) override {}

  void Show() override {}
  void ShowForProfile(Profile* profile) override {}
  void ShowForVoiceSearch(
      Profile* profile,
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble)
      override {}
  void HideCustomLauncherPage() override {}
  void ShowForAppInstall(Profile* profile,
                         const std::string& extension_id,
                         bool start_discovery_tracking) override {}
  void DismissAppList() override {}
  void ShowForCustomLauncherPage(Profile* profile) override {}

  Profile* GetCurrentAppListProfile() override { return nullptr; }
  bool IsAppListVisible() const override { return false; }
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override {}
  gfx::NativeWindow GetAppListWindow() override { return nullptr; }
  AppListControllerDelegate* GetControllerDelegate() override {
    return nullptr;
  }
  void CreateShortcut() override {}

  DISALLOW_COPY_AND_ASSIGN(AppListServiceDisabled);
};

}  // namespace

// static
AppListService* AppListService::Get() {
  return AppListServiceDisabled::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {}

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {}

// static
bool AppListService::HandleLaunchCommandLine(
    const base::CommandLine& command_line,
    Profile* launch_profile) {
  return false;
}
