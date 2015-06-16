// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/options_util.h"

#include "base/prefs/pref_service.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/sync_driver/sync_service.h"

namespace autofill {

bool WalletIntegrationAvailable(
    sync_driver::SyncService* sync_service,
    const PrefService& pref_service,
    const PersonalDataManager& personal_data_manager) {
  if (!(sync_service && sync_service->CanSyncStart() &&
        sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE))) {
    return false;
  }

  if (!personal_data_manager.IsExperimentalWalletIntegrationEnabled())
    return false;

  // If the user is signed in and the feature is enabled, but no data is being
  // synced, hide the option. The user doesn't have a Wallet account. If the
  // feature is disabled, we can't know, so show the checkbox.
  if (!pref_service.GetBoolean(prefs::kAutofillWalletImportEnabled))
    return true;

  // If wallet is preferred but we haven't gotten the sync data yet, we don't
  // know, so show the checkbox.
  if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA))
    return true;

  return personal_data_manager.HasServerData();
}

}  // namespace autofill
