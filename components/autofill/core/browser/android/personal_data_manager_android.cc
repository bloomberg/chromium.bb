// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Populates default autofill profile from user's own Android contact.

#include "components/autofill/core/browser/android/auxiliary_profile_loader_android.h"
#include "components/autofill/core/browser/android/auxiliary_profiles_android.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

void PersonalDataManager::LoadAuxiliaryProfiles(bool record_metrics) const {
  auxiliary_profiles_.clear();
  autofill::AuxiliaryProfileLoaderAndroid profile_loader;
  profile_loader.Init(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
  if (profile_loader.GetHasPermissions()) {
    autofill::AuxiliaryProfilesAndroid impl(profile_loader, app_locale_);
    auxiliary_profiles_.push_back(impl.LoadContactsProfile().release());
  }
}

}  // namespace autofill
