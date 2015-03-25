// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/options_util.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

namespace autofill {

bool WalletIntegrationAvailableForProfile(Profile* profile) {
  // TODO(estade): what to do in the IsManaged case?
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return service && service->IsSyncEnabledAndLoggedIn() &&
         service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE) &&
         service->GetRegisteredDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

}  // namespace autofill
