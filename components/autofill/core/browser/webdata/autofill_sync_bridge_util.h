// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_

#include <memory>
#include <string>

#include "components/sync/model/entity_change.h"
#include "components/sync/model/entity_data.h"

namespace autofill {

class AutofillProfile;
class AutofillTable;
class CreditCard;
struct PaymentsCustomerData;

// Returns the wallet metadata specifics id for the specified |metadata_id|.
std::string GetSpecificsIdForMetadataId(const std::string& metadata_id);

// Returns the storage key to be used for wallet metadata for the specified
// wallet metadata |specifics_id|.
std::string GetStorageKeyForWalletMetadataSpecificsId(
    const std::string& specifics_id);

// Returns the wallet metadata specifics storage key for the specified
// |metadata_id|.
std::string GetStorageKeyForMetadataId(const std::string& metadata_id);

// Sets the fields of the |wallet_specifics| based on the the specified
// |address|. If |enforce_utf8|, ids are encoded into UTF-8.
void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8 = false);

// Sets the fields of the |wallet_specifics| based on the the specified |card|.
// If |enforce_utf8|, ids are encoded into UTF-8.
void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    sync_pb::AutofillWalletSpecifics* wallet_specifics,
    bool enforce_utf8 = false);

// Sets the fields of the |wallet_specifics| based on the specified
// |customer_data|.
void SetAutofillWalletSpecificsFromPaymentsCustomerData(
    const PaymentsCustomerData& customer_data,
    sync_pb::AutofillWalletSpecifics* wallet_specifics);

// TODO(sebsg): This should probably copy the converted state for the address
// too.
// Copies the metadata from the local cards (if present) to the corresponding
// server cards so that they don't get overwritten. This is because the wallet
// data does not include those. They are handled by the
// AutofillWalletMetadataSyncBridge.
void CopyRelevantWalletMetadataFromDisk(
    const AutofillTable& table,
    std::vector<CreditCard>* cards_from_server);

// Populates the wallet datatypes from the sync data and uses the sync data to
// link the card to its billing address. If |wallet_addresses| is a nullptr,
// this function will not extract addresses.
void PopulateWalletTypesFromSyncData(
    const ::syncer::EntityChangeList& entity_data,
    std::vector<CreditCard>* wallet_cards,
    std::vector<AutofillProfile>* wallet_addresses,
    std::vector<PaymentsCustomerData>* customer_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
