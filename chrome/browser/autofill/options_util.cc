// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/options_util.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"

namespace autofill {

bool WalletIntegrationAvailableForProfile(Profile* profile) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!(service && service->IsSyncEnabledAndLoggedIn() &&
        service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE))) {
    return false;
  }

  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForProfile(profile);
  if (!pdm->IsExperimentalWalletIntegrationEnabled())
    return false;

  // If the user is signed in and the feature is enabled, but no data is being
  // synced, hide the option. The user doesn't have a Wallet account. If the
  // feature is disabled, we can't know, so show the checkbox.
  if (!profile->GetPrefs()->GetBoolean(prefs::kAutofillWalletImportEnabled))
    return true;

  // If wallet is preferred but we haven't gotten the sync data yet, we don't
  // know, so show the checkbox.
  if (!service->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA))
    return true;

  return pdm->HasServerData();
}

}  // namespace autofill
