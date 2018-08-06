// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_

#include <memory>
#include <string>

#include "components/sync/model/entity_data.h"

namespace autofill {

class AutofillProfile;
class CreditCard;

// Returns the wallet specifics id for the specified |server_id|.
std::string GetSpecificsIdForEntryServerId(const std::string& server_id);

// Returns the storage key for the specified |server_id|.
std::string GetStorageKeyForSpecificsId(const std::string& specifics_id);

// Returns the wallet specifics storage key for the specified |server_id|.
std::string GetStorageKeyForEntryServerId(const std::string& server_id);

// Returns the client tag for the specified wallet |type| and
// |wallet_data_specifics_id|.
std::string GetClientTagForSpecificsId(
    sync_pb::AutofillWalletSpecifics::WalletInfoType type,
    const std::string& wallet_data_specifics_id);

// Sets the fields of the |wallet_specifics| based on the the specified
// |address|.
void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Creates a EntityData object corresponding to the specified |address|.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromAutofillServerProfile(
    const AutofillProfile& address);

// Sets the fields of the |wallet_specifics| based on the the specified |card|.
void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// Creates a EntityData object corresponding to the specified |card|.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromCard(
    const CreditCard& card);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
