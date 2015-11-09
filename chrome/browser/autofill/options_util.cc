// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/options_util.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/autofill/core/browser/options_util.h"
#include "components/browser_sync/browser/profile_sync_service.h"

namespace autofill {

bool WalletIntegrationAvailableForProfile(Profile* profile) {
  return WalletIntegrationAvailable(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile),
      *profile->GetPrefs(),
      *PersonalDataManagerFactory::GetForProfile(profile));
}

}  // namespace autofill
