// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

namespace test {

namespace {

class CreateProfileHelper {
 public:
  CreateProfileHelper() : profile_(NULL) {}

  Profile* CreateAsync() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath temp_profile_dir =
        profile_manager->user_data_dir().AppendASCII("Profile 1");
    profile_manager->CreateProfileAsync(
        temp_profile_dir,
        base::Bind(&CreateProfileHelper::OnProfileCreated,
                   base::Unretained(this)),
        base::string16(),
        base::string16(),
        std::string());
    run_loop_.Run();
    return profile_;
  }

 private:
  void OnProfileCreated(Profile* profile, Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile_ = profile;
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CreateProfileHelper);
};

}  // namespace

app_list::AppListModel* GetAppListModel(AppListService* service) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(
      service->GetCurrentAppListProfile())->model();
}

AppListService* GetAppListService() {
  // TODO(tapted): Consider testing ash explicitly on the win-ash trybot.
  return AppListService::Get(chrome::GetActiveDesktop());
}

AppListServiceImpl* GetAppListServiceImpl() {
  // AppListServiceImpl is the only subclass of AppListService, which has pure
  // virtuals. So this must either be NULL, or an AppListServiceImpl.
  return static_cast<AppListServiceImpl*>(GetAppListService());
}

Profile* CreateSecondProfileAsync() {
  CreateProfileHelper helper;
  return helper.CreateAsync();
}

}  // namespace test
