// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

#if defined(TOOLKIT_VIEWS)
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/app_list/app_list_service_disabled_mac.h"
#endif

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

#if defined(TOOLKIT_VIEWS)
bool IsProfileSignedOut(Profile* profile) {
  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath(), &entry);
  return has_entry && entry->IsSigninRequired();
}

// Opens a Chrome browser tab at chrome://apps.
void OpenAppsPage(Profile* fallback_profile) {
  Browser* browser = chrome::FindLastActive();
  Profile* app_list_profile = browser ? browser->profile() : fallback_profile;
  app_list_profile = app_list_profile->GetOriginalProfile();

  if (IsProfileSignedOut(app_list_profile) ||
      app_list_profile->IsSystemProfile() ||
      app_list_profile->IsGuestSession()) {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    return;
  }

  NavigateParams params(app_list_profile, GURL(chrome::kChromeUIAppsURL),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  Navigate(&params);
}
#endif

}  // namespace

// static
AppListService* AppListService::Get() {
  return AppListServiceDisabled::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {
#if defined(OS_MACOSX)
  InitAppsPageLegacyShimHandler(&OpenAppsPage);
#endif
}

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {}

// static
bool AppListService::HandleLaunchCommandLine(
    const base::CommandLine& command_line,
    Profile* launch_profile) {
#if defined(TOOLKIT_VIEWS)
  if (!command_line.HasSwitch(switches::kShowAppList))
    return false;

  OpenAppsPage(launch_profile);
  return true;
#else
  return false;
#endif
}
