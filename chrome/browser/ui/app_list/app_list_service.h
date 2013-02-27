// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
}

class AppListService : public ProfileInfoCacheObserver {
 public:
  // Get the AppListService for the current platform and desktop type.
  static AppListService* Get();

  // Call Init for all AppListService instances on this platform.
  static void InitAll(Profile* initial_profile);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual base::FilePath GetAppListProfilePath(
      const base::FilePath& user_data_dir);

  // Show the app list.
  virtual void ShowAppList(Profile* profile);

  // Dismiss the app list.
  virtual void DismissAppList();

  virtual void SetAppListProfile(const base::FilePath& profile_file_path);

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile();

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const;

  // ProfileInfoCacheObserver overrides:
  virtual void OnProfileAdded(const base::FilePath& profilePath) OVERRIDE;
  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(const base::FilePath& profile_path,
                                    const string16& profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(
      const base::FilePath& profile_path) OVERRIDE;

 protected:
  AppListService() {}
  virtual ~AppListService() {}

  // Do any once off initialization needed for the app list.
  virtual void Init(Profile* initial_profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListService);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
