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
    bool has_persistent_storage,
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_DATA,
              /*dump_stack=*/base::RepeatingClosure()),
          has_persistent_storage, web_data_backend));
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
    bool has_persistent_storage,
    AutofillWebDataBackend* web_data_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      has_persistent_storage_(has_persistent_storage),
      initial_sync_done_(false),
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
  SetSyncData(entity_data);

  // After the first sync, we are sure that initial sync is done.
  initial_sync_done_ = true;
  return base::nullopt;
}

base::Optional<syncer::ModelError> AutofillWalletSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This bridge does not support incremental updates, so whenever this is
  // called, the change list should be empty.
  DCHECK(entity_data.empty()) << "Received an unsupported incremental update.";
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

bool AutofillWalletSyncBridge::SupportsIncrementalUpdates() const {
  // The payments server always returns the full dataset whenever there's any
  // change to the user's payments data. Therefore, we don't implement full
  // incremental-update support in this bridge, and clear all data
  // before inserting new instead.
  return false;
}

AutofillWalletSyncBridge::StopSyncResponse
AutofillWalletSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // If a metadata change list gets passed in, that means sync is actually
  // disabled, so we want to delete the payments data.
  if (delete_metadata_change_list) {
    SetSyncData(syncer::EntityChangeList());
  }
  return StopSyncResponse::kModelStillReadyToSync;
}

void AutofillWalletSyncBridge::SetSyncData(
    const syncer::EntityChangeList& entity_data) {
  bool wallet_data_changed = false;

  // Extract the Autofill types from the sync |entity_data|.
  std::vector<CreditCard> wallet_cards;
  std::vector<PaymentsCustomerData> customer_data;
  if (has_persistent_storage_) {
    // When in persistent storage mode, we update wallet addresses.
    std::vector<AutofillProfile> wallet_addresses;
    PopulateWalletTypesFromSyncData(entity_data, &wallet_cards,
                                    &wallet_addresses, &customer_data);
    wallet_data_changed |= SetWalletAddresses(std::move(wallet_addresses));
  } else {
    // When in ephemeral storage mode, we ignore wallet addresses.
    PopulateWalletTypesFromSyncData(entity_data, &wallet_cards, nullptr,
                                    &customer_data);
  }

  // In both cases, we need to update wallet cards and payments customer data.
  wallet_data_changed |= SetWalletCards(std::move(wallet_cards));
  wallet_data_changed |= SetPaymentsCustormerData(std::move(customer_data));

  if (web_data_backend_ && wallet_data_changed)
    web_data_backend_->NotifyOfMultipleAutofillChanges();
}

bool AutofillWalletSyncBridge::SetWalletCards(
    std::vector<CreditCard> wallet_cards) {
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
  AutofillWalletDiff diff =
      ComputeAutofillWalletDiff(existing_cards, wallet_cards);

  // Record only local changes that correspond to changes in the payments
  // backend and not local changes due to initial sync.
  if (initial_sync_done_) {
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAdded", diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsRemoved", diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletCardsAddedOrRemoved",
                             diff.items_added + diff.items_removed);
  }

  if (!diff.IsEmpty()) {
    table->SetServerCreditCards(wallet_cards);
    return true;
  }
  return false;
}

bool AutofillWalletSyncBridge::SetWalletAddresses(
    std::vector<AutofillProfile> wallet_addresses) {
  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  AutofillTable* table = GetAutofillTable();
  std::vector<std::unique_ptr<AutofillProfile>> existing_addresses;
  table->GetServerProfiles(&existing_addresses);
  AutofillWalletDiff diff =
      ComputeAutofillWalletDiff(existing_addresses, wallet_addresses);

  // Record only local changes that correspond to changes in the payments
  // backend and not local changes due to initial sync.
  if (initial_sync_done_) {
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesAdded", diff.items_added);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesRemoved",
                             diff.items_removed);
    UMA_HISTOGRAM_COUNTS_100("Autofill.WalletAddressesAddedOrRemoved",
                             diff.items_added + diff.items_removed);
  }

  if (!diff.IsEmpty()) {
    table->SetServerProfiles(wallet_addresses);
    return true;
  }
  return false;
}

bool AutofillWalletSyncBridge::SetPaymentsCustormerData(
    std::vector<PaymentsCustomerData> customer_data) {
  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  AutofillTable* table = GetAutofillTable();
  std::unique_ptr<PaymentsCustomerData> existing_entry;
  table->GetPaymentsCustomerData(&existing_entry);

  // In case there were multiple entries (and there shouldn't!), we take the
  // pointer to the first entry in the vector.
  PaymentsCustomerData* new_entry =
      customer_data.empty() ? nullptr : customer_data.data();

#if DCHECK_IS_ON()
  if (customer_data.size() > 1) {
    DLOG(WARNING) << "Sync wallet_data update has " << customer_data.size()
                  << " payments-customer-data entries; expected 0 or 1.";
  }
#endif  // DCHECK_IS_ON()

  if (!new_entry && existing_entry) {
    // Clear the existing entry in the DB.
    GetAutofillTable()->SetPaymentsCustomerData(nullptr);
    return true;
  } else if (new_entry && (!existing_entry || *new_entry != *existing_entry)) {
    // Write the new entry in the DB as it differs from the existing one.
    GetAutofillTable()->SetPaymentsCustomerData(new_entry);
    return true;
  }
  return false;
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
  initial_sync_done_ = batch->GetModelTypeState().initial_sync_done();

  change_processor()->ModelReadyToSync(std::move(batch));
}

}  // namespace autofill
