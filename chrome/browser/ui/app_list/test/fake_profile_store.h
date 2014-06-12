// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_

#include <map>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/profile_store.h"

class FakeProfileStore : public ProfileStore {
 public:
  explicit FakeProfileStore(const base::FilePath& user_data_dir);
  virtual ~FakeProfileStore();

  void LoadProfile(Profile* profile);
  void RemoveProfile(Profile* profile);

  // ProfileStore overrides.
  virtual void AddProfileObserver(ProfileInfoCacheObserver* observer) OVERRIDE;
  virtual void LoadProfileAsync(
      const base::FilePath& path,
      base::Callback<void(Profile*)> callback) OVERRIDE;
  virtual Profile* GetProfileByPath(const base::FilePath& path) OVERRIDE;
  virtual base::FilePath GetUserDataDir() OVERRIDE;
  virtual bool IsProfileSupervised(const base::FilePath& path) OVERRIDE;

 private:
  base::FilePath user_data_dir_;
  typedef std::map<base::FilePath, base::Callback<void(Profile*)> >
      CallbacksByPath;
  CallbacksByPath callbacks_;
  ObserverList<ProfileInfoCacheObserver> observer_list_;
  typedef std::map<base::FilePath, Profile*> ProfilesByPath;
  ProfilesByPath loaded_profiles_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_
