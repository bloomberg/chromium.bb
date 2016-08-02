// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/profile_loader.h"

class AppListViewDelegate;
class ProfileStore;

namespace base {
class FilePath;
}

namespace test {
class AppListServiceImplTestApi;
}

// Parts of the AppListService implementation shared between platforms.
class AppListServiceImpl : public AppListService,
                           public ProfileAttributesStorage::Observer {
 public:
  ~AppListServiceImpl() override;

  // Constructor used for testing.
  AppListServiceImpl(const base::CommandLine& command_line,
                     PrefService* local_state,
                     std::unique_ptr<ProfileStore> profile_store);

  // Lazily create the Chrome AppListViewDelegate and ensure it is set to the
  // given |profile|.
  AppListViewDelegate* GetViewDelegate(Profile* profile);

  void RecordAppListLaunch();
  static void RecordAppListAppLaunch();

  // AppListService overrides:
  void SetAppListNextPaintCallback(void (*callback)()) override;
  void Init(Profile* initial_profile) override;
  base::FilePath GetProfilePath(const base::FilePath& user_data_dir) override;
  void SetProfilePath(const base::FilePath& profile_path) override;
  void Show() override;
  void ShowForVoiceSearch(
      Profile* profile,
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble)
      override;
  void ShowForAppInstall(Profile* profile,
                         const std::string& extension_id,
                         bool start_discovery_tracking) override;
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override;
  void CreateShortcut() override;

 protected:
  AppListServiceImpl();

  // Create the app list UI, and maintain its state, but do not show it.
  virtual void CreateForProfile(Profile* requested_profile) = 0;

  // Destroy the app list. Called when the profile that the app list is showing
  // is being deleted.
  virtual void DestroyAppList() = 0;

  void InvalidatePendingProfileLoads();
  ProfileLoader& profile_loader() { return *profile_loader_; }
  const ProfileLoader& profile_loader() const { return *profile_loader_; }

  // Perform startup checks shared between desktop implementations of the app
  // list. Currently this checks command line flags to enable or disable the app
  // list, and records UMA stats delayed from a previous Chrome process.
  void PerformStartupChecks(Profile* initial_profile);

 private:
  friend class test::AppListServiceImplTestApi;
  static void SendAppListStats();

  std::string GetProfileName();

  // ProfileAttributesStorage::Observer overrides:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  std::unique_ptr<ProfileStore> profile_store_;
  base::CommandLine command_line_;
  PrefService* local_state_;
  std::unique_ptr<ProfileLoader> profile_loader_;
  std::unique_ptr<AppListViewDelegate> view_delegate_;

  base::WeakPtrFactory<AppListServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
