// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/profile_loader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"

ProfileLoader::ProfileLoader(ProfileManager* profile_manager)
  : profile_manager_(profile_manager),
    profile_load_sequence_id_(0),
    pending_profile_loads_(0),
    weak_factory_(this) {
}

ProfileLoader::~ProfileLoader() {
}

bool ProfileLoader::AnyProfilesLoading() const {
  return pending_profile_loads_ > 0;
}

void ProfileLoader::InvalidatePendingProfileLoads() {
  profile_load_sequence_id_++;
}

void ProfileLoader::LoadProfileInvalidatingOtherLoads(
    const base::FilePath& profile_file_path,
    base::Callback<void(Profile*)> callback) {
  Profile* profile = profile_manager_->GetProfileByPath(profile_file_path);
  if (profile) {
    callback.Run(profile);
    return;
  }

  IncrementPendingProfileLoads();
  profile_manager_->CreateProfileAsync(
      profile_file_path,
      base::Bind(&ProfileLoader::OnProfileLoaded,
                 weak_factory_.GetWeakPtr(),
                 profile_load_sequence_id_,
                 callback),
      string16(), string16(), false);
}

void ProfileLoader::OnProfileLoaded(int profile_load_sequence_id,
                                    base::Callback<void(Profile*)> callback,
                                    Profile* profile,
                                    Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      if (profile_load_sequence_id == profile_load_sequence_id_)
        callback.Run(profile);
      DecrementPendingProfileLoads();
      break;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
      DecrementPendingProfileLoads();
      break;
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED();
      break;
  }
}

void ProfileLoader::IncrementPendingProfileLoads() {
  pending_profile_loads_++;
  if (pending_profile_loads_ == 1)
    chrome::StartKeepAlive();
}

void ProfileLoader::DecrementPendingProfileLoads() {
  pending_profile_loads_--;
  if (pending_profile_loads_ == 0)
    chrome::EndKeepAlive();
}
