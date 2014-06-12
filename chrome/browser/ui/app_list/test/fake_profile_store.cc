// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_profile_store.h"

#include "chrome/browser/profiles/profile.h"

FakeProfileStore::FakeProfileStore(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {
}

FakeProfileStore::~FakeProfileStore() {
}

void FakeProfileStore::LoadProfile(Profile* profile) {
  loaded_profiles_[profile->GetPath()] = profile;
  CallbacksByPath::iterator it = callbacks_.find(profile->GetPath());
  if (it != callbacks_.end()) {
    it->second.Run(profile);
    callbacks_.erase(it);
  }
}

void FakeProfileStore::RemoveProfile(Profile* profile) {
  base::FilePath path(profile->GetPath());
  FOR_EACH_OBSERVER(ProfileInfoCacheObserver, observer_list_,
                    OnProfileWillBeRemoved(path));
  loaded_profiles_.erase(path);
  FOR_EACH_OBSERVER(ProfileInfoCacheObserver, observer_list_,
                    OnProfileWasRemoved(path, base::string16()));
}

void FakeProfileStore::AddProfileObserver(
    ProfileInfoCacheObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeProfileStore::LoadProfileAsync(
    const base::FilePath& path,
    base::Callback<void(Profile*)> callback) {
  Profile* profile = GetProfileByPath(path);
  if (profile) {
    callback.Run(profile);
    return;
  }
  callbacks_[path] = callback;
}

Profile* FakeProfileStore::GetProfileByPath(
    const base::FilePath& path) {
  ProfilesByPath::const_iterator it = loaded_profiles_.find(path);
  if (it != loaded_profiles_.end())
    return it->second;
  return NULL;
}

base::FilePath FakeProfileStore::GetUserDataDir() {
  return user_data_dir_;
}

bool FakeProfileStore::IsProfileSupervised(const base::FilePath& path) {
  return false;
}
