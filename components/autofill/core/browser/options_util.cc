// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/options_util.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/sync_driver/sync_service.h"

namespace autofill {

bool WalletIntegrationAvailable(
    sync_driver::SyncService* sync_service,
    const PersonalDataManager& personal_data_manager) {
  if (!(sync_service && sync_service->CanSyncStart() &&
        sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE))) {
    return false;
  }

  return personal_data_manager.IsExperimentalWalletIntegrationEnabled();
}

}  // namespace autofill
