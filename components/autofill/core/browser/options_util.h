// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_OPTIONS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_OPTIONS_UTIL_H_

class PrefService;

namespace sync_driver {
class SyncService;
}

namespace autofill {

class PersonalDataManager;

// Decides whether the Autofill Wallet integration is available (i.e. can be
// enabled or disabled by the user).
bool WalletIntegrationAvailable(
    sync_driver::SyncService* sync_service,
    const PrefService& pref_service,
    const PersonalDataManager& personal_data_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_OPTIONS_UTIL_H_
