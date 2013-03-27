// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Populates default autofill profile from user's own Android contact.

#include "components/autofill/browser/android/auxiliary_profile_loader_android.h"
#include "components/autofill/browser/android/auxiliary_profiles_android.h"
#include "components/autofill/browser/personal_data_manager.h"

void PersonalDataManager::LoadAuxiliaryProfiles() {
  auxiliary_profiles_.clear();
  autofill::AuxiliaryProfileLoaderAndroid profile_loader;
  profile_loader.Init(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
  if (profile_loader.GetHasPermissions()) {
    autofill::AuxiliaryProfilesAndroid impl(profile_loader);
    auxiliary_profiles_.push_back(impl.LoadContactsProfile().release());
  }
}
