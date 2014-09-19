// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
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
                           public ProfileInfoCacheObserver {
 public:
  virtual ~AppListServiceImpl();

  // Constructor used for testing.
  AppListServiceImpl(const base::CommandLine& command_line,
                     PrefService* local_state,
                     scoped_ptr<ProfileStore> profile_store);

  // Lazily create the Chrome AppListViewDelegate and ensure it is set to the
  // given |profile|.
  AppListViewDelegate* GetViewDelegate(Profile* profile);

  void RecordAppListLaunch();
  static void RecordAppListAppLaunch();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(void (*callback)()) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) OVERRIDE;
  virtual void SetProfilePath(const base::FilePath& profile_path) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void AutoShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void EnableAppList(Profile* initial_profile,
                             AppListEnableSource enable_source) OVERRIDE;
  virtual void CreateShortcut() OVERRIDE;

 protected:
  AppListServiceImpl();

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

  // Loads a profile asynchronously and calls OnProfileLoaded() when done.
  void LoadProfileAsync(const base::FilePath& profile_file_path);

  // Callback for asynchronous profile load.
  void OnProfileLoaded(int profile_load_sequence_id,
                       Profile* profile,
                       Profile::CreateStatus status);

  // ProfileInfoCacheObserver overrides:
  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE;

  scoped_ptr<ProfileStore> profile_store_;
  base::CommandLine command_line_;
  PrefService* local_state_;
  scoped_ptr<ProfileLoader> profile_loader_;
  scoped_ptr<AppListViewDelegate> view_delegate_;

  base::WeakPtrFactory<AppListServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
