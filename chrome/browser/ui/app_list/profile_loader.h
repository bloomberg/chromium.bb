// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_
#define CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"

namespace base {
class FilePath;
}

class ProfileManager;

class ProfileLoader {
 public:
  explicit ProfileLoader(ProfileManager* profile_manager);
  ~ProfileLoader();

  bool AnyProfilesLoading() const;
  void InvalidatePendingProfileLoads();
  void LoadProfileInvalidatingOtherLoads(
      const base::FilePath& profile_file_path,
      base::Callback<void(Profile*)> callback);

 private:
  void OnProfileLoaded(int profile_load_sequence_id,
                       base::Callback<void(Profile*)> callback,
                       Profile* profile,
                       Profile::CreateStatus status);

  void IncrementPendingProfileLoads();
  void DecrementPendingProfileLoads();

 private:
  ProfileManager* profile_manager_;
  int profile_load_sequence_id_;
  int pending_profile_loads_;

  base::WeakPtrFactory<ProfileLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_
