// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_loader.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

namespace base {
class FilePath;
}

// Parts of the AppListService implementation shared between platforms.
class AppListServiceImpl : public AppListService,
                           public ProfileInfoCacheObserver {
 public:
  static void RecordAppListLaunch();
  static void RecordAppListAppLaunch();
  static void SendAppListStats();

 protected:
  AppListServiceImpl();
  virtual ~AppListServiceImpl();

  Profile* profile() const { return profile_; }
  void SetProfile(Profile* new_profile);
  void InvalidatePendingProfileLoads();
  ProfileLoader& profile_loader() { return profile_loader_; }
  const ProfileLoader& profile_loader() const { return profile_loader_; }

  // Process command line flags shared between desktop implementations of the
  // app list. Currently this allows for enabling or disabling the app list.
  void HandleCommandLineFlags(Profile* initial_profile);

  // Create a platform-specific shortcut for the app list.
  virtual void CreateShortcut();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;

  // Returns the app list path configured in BrowserProcess::local_state().
  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) OVERRIDE;
  virtual void SetProfilePath(const base::FilePath& profile_path) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void EnableAppList(Profile* initial_profile) OVERRIDE;

 private:
  // Loads a profile asynchronously and calls OnProfileLoaded() when done.
  void LoadProfileAsync(const base::FilePath& profile_file_path);

  // Callback for asynchronous profile load.
  void OnProfileLoaded(int profile_load_sequence_id,
                       Profile* profile,
                       Profile::CreateStatus status);

  virtual Profile* GetCurrentAppListProfile() OVERRIDE;

  // ProfileInfoCacheObserver overrides:
  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE;

  // The profile the AppList is currently displaying.
  Profile* profile_;

  // Incremented to indicate that pending profile loads are no longer valid.
  int profile_load_sequence_id_;

  // How many profile loads are pending.
  int pending_profile_loads_;

  base::WeakPtrFactory<AppListServiceImpl> weak_factory_;

  ProfileLoader profile_loader_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
