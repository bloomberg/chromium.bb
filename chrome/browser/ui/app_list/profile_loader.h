// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_
#define CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/keep_alive_service.h"
#include "chrome/browser/ui/app_list/profile_store.h"

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
// TODO(koz): Merge this into AppListServiceImpl.
class ProfileLoader {
 public:
  explicit ProfileLoader(ProfileStore* profile_store,
                         scoped_ptr<KeepAliveService> keep_alive_service);
  ~ProfileLoader();

  bool IsAnyProfileLoading() const;
  void InvalidatePendingProfileLoads();
  void LoadProfileInvalidatingOtherLoads(
      const base::FilePath& profile_file_path,
      base::Callback<void(Profile*)> callback);

 private:
  void OnProfileLoaded(int profile_load_sequence_id,
                       base::Callback<void(Profile*)> callback,
                       Profile* profile);

  void IncrementPendingProfileLoads();
  void DecrementPendingProfileLoads();

  ProfileStore* profile_store_;
  scoped_ptr<KeepAliveService> keep_alive_service_;
  int profile_load_sequence_id_;
  int pending_profile_loads_;

  base::WeakPtrFactory<ProfileLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_PROFILE_LOADER_H_
