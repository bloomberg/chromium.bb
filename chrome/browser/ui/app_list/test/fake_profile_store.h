// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_

#include <map>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/app_list/profile_store.h"

class PrefService;

class FakeProfileStore : public ProfileStore {
 public:
  FakeProfileStore(const base::FilePath& user_data_dir,
                   PrefService* local_state);
  ~FakeProfileStore() override;

  void LoadProfile(Profile* profile);
  void RemoveProfile(Profile* profile);

  // ProfileStore overrides.
  void AddProfileObserver(ProfileAttributesStorage::Observer* observer)
      override;
  void LoadProfileAsync(const base::FilePath& path,
                        base::Callback<void(Profile*)> callback) override;
  Profile* GetProfileByPath(const base::FilePath& path) override;
  base::FilePath GetUserDataDir() override;
  std::string GetLastUsedProfileName() override;
  bool IsProfileSupervised(const base::FilePath& path) override;
  bool IsProfileLocked(const base::FilePath& path) override;

 private:
  base::FilePath user_data_dir_;
  PrefService* local_state_;
  typedef std::map<base::FilePath, base::Callback<void(Profile*)> >
      CallbacksByPath;
  CallbacksByPath callbacks_;
  base::ObserverList<ProfileAttributesStorage::Observer> observer_list_;
  typedef std::map<base::FilePath, Profile*> ProfilesByPath;
  ProfilesByPath loaded_profiles_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_STORE_H_
