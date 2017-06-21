// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace lock_screen_apps {

AppManagerImpl::AppManagerImpl() = default;

AppManagerImpl::~AppManagerImpl() = default;

void AppManagerImpl::Initialize(Profile* primary_profile,
                                Profile* lock_screen_profile) {
  DCHECK(primary_profile);
  DCHECK(lock_screen_profile);
  DCHECK_NE(primary_profile, lock_screen_profile);
  // Do not use OTR profile for lock screen apps. This is important for
  // profile usage in |LaunchNoteTaking| - lock screen app background page runs
  // in original, non off the record profile, so the launch event has to be
  // dispatched to that profile. For other |lock_screen_profile_|, it makes no
  // difference - the profile is used to get browser context keyed services, all
  // of which redirect OTR profile to the original one.
  DCHECK(!lock_screen_profile->IsOffTheRecord());

  CHECK(!chromeos::ProfileHelper::Get()->GetUserByProfile(lock_screen_profile))
      << "Lock screen profile should not be associated with any users.";

  primary_profile_ = primary_profile;
  lock_screen_profile_ = lock_screen_profile;
}

void AppManagerImpl::Start(const base::Closure& note_taking_changed_callback) {
  note_taking_changed_callback_ = note_taking_changed_callback;
}

void AppManagerImpl::Stop() {}

bool AppManagerImpl::IsNoteTakingAppAvailable() const {
  return false;
}

std::string AppManagerImpl::GetNoteTakingAppId() const {
  return std::string();
}

bool AppManagerImpl::LaunchNoteTaking() {
  return false;
}

}  // namespace lock_screen_apps
