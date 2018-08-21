// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using sync_pb::AutofillWalletSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletSyncBridgeUserDataKey = 0;

std::string GetSpecificsIdFromAutofillWalletSpecifics(
    const AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return specifics.masked_card().id();
    case AutofillWalletSpecifics::POSTAL_ADDRESS:
      return specifics.address().id();
    case AutofillWalletSpecifics::CUSTOMER_DATA:
      return specifics.customer_data().id();
    case AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

std::string GetClientTagForWalletDataSpecificsId(
    const std::string& specifics_id) {
  // Unlike for the wallet_metadata model type, the wallet_data expects
  // specifics id directly as client tags.
  return specifics_id;
}

}  // namespace

// static
void AutofillWalletSyncBridge::CreateForWebDataServiceAndBackend(
    const std::string& app_locale,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure()),
          web_data_backend));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletSyncBridgeUserDataKey));
}

AutofillWalletSyncBridge::AutofillWalletSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(web_data_backend) {
  DCHECK(web_data_backend_);

  LoadMetadata();
}

AutofillWalletSyncBridge::~AutofillWalletSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL_WALLET_DATA);
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  SetSyncData(entity_data, /*is_initial_data=*/true);
  return base::nullopt;
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  SetSyncData(entity_data, /*is_initial_data=*/false);
  return base::nullopt;
}

void AutofillWalletSyncBridge::GetData(StorageKeyList storage_keys,
                                       DataCallback callback) {
  // This data type is never synced "up" so we don't need to implement this.
  NOTIMPLEMENTED();
}

void AutofillWalletSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> cards;
  std::unique_ptr<PaymentsCustomerData> customer_data;
  if (!GetAutofillTable()->GetServerProfiles(&profiles) ||
      !GetAutofillTable()->GetServerCreditCards(&cards) ||
      !GetAutofillTable()->GetPaymentsCustomerData(&customer_data)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::unique_ptr<AutofillProfile>& entry : profiles) {
    batch->Put(GetStorageKeyForEntryServerId(entry->server_id()),
               CreateEntityDataFromAutofillServerProfile(*entry));
  }
  for (const std::unique_ptr<CreditCard>& entry : cards) {
    batch->Put(GetStorageKeyForEntryServerId(entry->server_id()),
               CreateEntityDataFromCard(*entry));
  }

  if (customer_data) {
    batch->Put(GetStorageKeyForEntryServerId(customer_data->customer_id),
               CreateEntityDataFromPaymentsCustomerData(*customer_data));
  }
  std::move(callback).Run(std::move(batch));
}

std::string AutofillWalletSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());

  return GetClientTagForWalletDataSpecificsId(
      GetSpecificsIdFromAutofillWalletSpecifics(
          entity_data.specifics.autofill_wallet()));
}

std::string AutofillWalletSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill_wallet());
  return GetStorageKeyForSpecificsId(GetSpecificsIdFromAutofillWalletSpecifics(
      entity_data.specifics.autofill_wallet()));
}

void AutofillWalletSyncBridge::SetSyncData(
    const syncer::EntityChangeList& entity_data,
    bool is_initial_data) {
  // Extract the Autofill types from the sync |entity_data|.
  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  std::vector<PaymentsCustomerData> customer_data;
  PopulateWalletTypesFromSyncData(entity_data, &wallet_cards, &wallet_addresses,
                                  &customer_data);

  // Users can set billing address of the server credit card locally, but that
  // information does not propagate to either Chrome Sync or Google Payments
  // server. To preserve user's preferred billing address and most recent use
  // stats, copy them from disk into |wallet_cards|.
  AutofillTable* table = GetAutofillTable();
  CopyRelevantWalletMetadataFromDisk(*table, &wallet_cards);

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  std::vector<std::unique_ptr<CreditCard>> existing_cards;
  table->GetServerCreditCards(&existing_cards);
  AutofillWalletDiff cards_diff =
      ComputeAutofillWalletDiff(existing_cards, wallet_cards);
  if (!cards_diff.IsEmpty())
    table->SetServerCreditCards(wallet_cards);

  std::vector<std::unique_ptr<AutofillProfile>> existing_addresses;
  table->GetServerProfiles(&existing_addresses);
  AutofillWalletDiff addresses_diff =
      ComputeAutofillWalletDiff(existing_addresses, wallet_addresses);
  if (!addresses_diff.IsEmpty())
    table->SetServerProfiles(wallet_addresses);

  if (customer_data.empty()) {
    // Clears the data only.
    table->SetPaymentsCustomerData(nullptr);
  } else {
    // In case there were multiple entries (and there shouldn't!), we take the
    // first entry in the vector.
    table->SetPaymentsCustomerData(&customer_data.front());
  }

  if (!is_initial_data) {
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAdded",
                             cards_diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsRemoved",
                             cards_diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAddedOrRemoved",
                             cards_diff.items_added + cards_diff.items_removed);

    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesAdded",
                             addresses_diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesRemoved",
                             addresses_diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100(
        "Autofill.WalletAddressesAddedOrRemoved",
        addresses_diff.items_added + addresses_diff.items_removed);
  }

  if (web_data_backend_ && (!cards_diff.IsEmpty() || !addresses_diff.IsEmpty()))
    web_data_backend_->NotifyOfMultipleAutofillChanges();
}

// static
template <class Item>
AutofillWalletSyncBridge::AutofillWalletDiff
AutofillWalletSyncBridge::ComputeAutofillWalletDiff(
    const std::vector<std::unique_ptr<Item>>& old_data,
    const std::vector<Item>& new_data) {
  // Build vectors of pointers, so that we can mutate (sort) them.
  std::vector<const Item*> old_ptrs;
  old_ptrs.reserve(old_data.size());
  for (const std::unique_ptr<Item>& old_item : old_data)
    old_ptrs.push_back(old_item.get());
  std::vector<const Item*> new_ptrs;
  new_ptrs.reserve(new_data.size());
  for (const Item& new_item : new_data)
    new_ptrs.push_back(&new_item);

  // Sort our vectors.
  auto compare = [](const Item* lhs, const Item* rhs) {
    return lhs->Compare(*rhs) < 0;
  };
  std::sort(old_ptrs.begin(), old_ptrs.end(), compare);
  std::sort(new_ptrs.begin(), new_ptrs.end(), compare);

  // Walk over both of them and count added/removed elements.
  AutofillWalletDiff result;
  auto old_it = old_ptrs.begin();
  auto new_it = new_ptrs.begin();
  while (old_it != old_ptrs.end()) {
    if (new_it == new_ptrs.end()) {
      result.items_removed += std::distance(old_it, old_ptrs.end());
      break;
    }
    int cmp = (*old_it)->Compare(**new_it);
    if (cmp < 0) {
      ++result.items_removed;
      ++old_it;
    } else if (cmp == 0) {
      ++old_it;
      ++new_it;
    } else {
      ++result.items_added;
      ++new_it;
    }
  }
  result.items_added += std::distance(new_it, new_ptrs.end());

  DCHECK_EQ(old_data.size() + result.items_added - result.items_removed,
            new_data.size());

  return result;
}

AutofillTable* AutofillWalletSyncBridge::GetAutofillTable() {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

void AutofillWalletSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL_WALLET_DATA,
                                              batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
