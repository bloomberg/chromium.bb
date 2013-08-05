// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LOADER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LOADER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace base {
class FilePath;
}

class ProfileManager;

// This class loads profiles asynchronously and calls the provided callback once
// the profile is ready. Only the callback for the most recent load request is
// called, and only if the load was successful.
//
// This is useful for loading profiles in response to user interaction where
// only the latest requested profile is required.
class ProfileLoader {
 public:
  explicit ProfileLoader(ProfileManager* profile_manager);
  ~ProfileLoader();

  bool IsAnyProfileLoading() const;
  void InvalidatePendingProfileLoads();
  void LoadProfileInvalidatingOtherLoads(
      const base::FilePath& profile_file_path,
      base::Callback<void(Profile*)> callback);

 protected:
  // These just call through to the ProfileManager.
  // Virtual so they can be mocked out in tests.
  virtual Profile* GetProfileByPath(const base::FilePath& path);
  virtual void CreateProfileAsync(
      const base::FilePath& profile_path,
      const ProfileManager::CreateCallback& callback,
      const string16& name,
      const string16& icon_url,
      const std::string& managed_user_id);

 private:
  void OnProfileLoaded(int profile_load_sequence_id,
                       base::Callback<void(Profile*)> callback,
                       Profile* profile,
                       Profile::CreateStatus status);

  void IncrementPendingProfileLoads();
  void DecrementPendingProfileLoads();

  ProfileManager* profile_manager_;
  int profile_load_sequence_id_;
  int pending_profile_loads_;

  base::WeakPtrFactory<ProfileLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LOADER_H_
