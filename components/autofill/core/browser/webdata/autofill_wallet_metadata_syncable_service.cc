// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace autofill {

namespace {

void* UserDataKey() {
  // Use the address of a static so that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

// Returns syncable metadata for the |local| profile or credit card.
syncer::SyncData BuildSyncData(sync_pb::WalletMetadataSpecifics::Type type,
                               const std::string& server_id,
                               const AutofillDataModel& local) {
  sync_pb::EntitySpecifics entity;
  sync_pb::WalletMetadataSpecifics* metadata = entity.mutable_wallet_metadata();
  metadata->set_type(type);
  metadata->set_id(server_id);
  metadata->set_use_count(local.use_count());
  metadata->set_use_date(local.use_date().ToInternalValue());

  std::string sync_tag;
  switch (type) {
    case sync_pb::WalletMetadataSpecifics::ADDRESS:
      sync_tag = "address-" + server_id;
      break;
    case sync_pb::WalletMetadataSpecifics::CARD:
      sync_tag = "card-" + server_id;
      break;
    case sync_pb::WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      break;
  }

  return syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity);
}

}  // namespace

AutofillWalletMetadataSyncableService::
    ~AutofillWalletMetadataSyncableService() {
}

syncer::SyncMergeResult
AutofillWalletMetadataSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_);
  DCHECK(!sync_error_factory_);
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  return MergeData(initial_sync_data);
}

void AutofillWalletMetadataSyncableService::StopSyncing(
    syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_.reset();
  sync_error_factory_.reset();
  cache_.clear();
}

syncer::SyncDataList AutofillWalletMetadataSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  syncer::SyncDataList data_list;
  std::map<std::string, AutofillProfile*> profiles;
  std::map<std::string, CreditCard*> cards;
  if (GetLocalData(&profiles, &cards)) {
    for (const auto& it : profiles) {
      data_list.push_back(BuildSyncData(
          sync_pb::WalletMetadataSpecifics::ADDRESS, it.first, *it.second));
    }

    for (const auto& it : cards) {
      data_list.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                        it.first, *it.second));
    }
  }

  return data_list;
}

syncer::SyncError AutofillWalletMetadataSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& changes_from_sync) {
  DCHECK(thread_checker_.CalledOnValidThread());

  cache_.clear();

  std::map<std::string, AutofillProfile*> profiles;
  std::map<std::string, CreditCard*> cards;
  GetLocalData(&profiles, &cards);

  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncChange& change : changes_from_sync) {
    const sync_pb::WalletMetadataSpecifics& remote_metadata =
        change.sync_data().GetSpecifics().wallet_metadata();
    switch (change.change_type()) {
      // Do not immediately delete data.
      case syncer::SyncChange::ACTION_ADD:
      // Intentional fall through.
      case syncer::SyncChange::ACTION_UPDATE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS:
            MergeRemote(change.sync_data(), address_updater, &profiles,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::CARD:
            MergeRemote(change.sync_data(), card_updater, &cards,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      // Undelete data immediately.
      case syncer::SyncChange::ACTION_DELETE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS: {
            const auto& it = profiles.find(remote_metadata.id());
            if (it != profiles.end()) {
              cache_.push_back(
                  BuildSyncData(sync_pb::WalletMetadataSpecifics::ADDRESS,
                                it->first, *it->second));
              changes_to_sync.push_back(syncer::SyncChange(
                  FROM_HERE, syncer::SyncChange::ACTION_ADD, cache_.back()));
              profiles.erase(it);
            }
            break;
          }

          case sync_pb::WalletMetadataSpecifics::CARD: {
            const auto& it = cards.find(remote_metadata.id());
            if (it != cards.end()) {
              cache_.push_back(
                  BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                it->first, *it->second));
              changes_to_sync.push_back(syncer::SyncChange(
                  FROM_HERE, syncer::SyncChange::ACTION_ADD, cache_.back()));
              cards.erase(it);
            }
            break;
          }

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      case syncer::SyncChange::ACTION_INVALID:
        NOTREACHED();
        break;
    }
  }

  // The remainder of |profiles| were not listed in |changes_from_sync|.
  for (const auto& it : profiles) {
    cache_.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::ADDRESS,
                                   it.first, *it.second));
  }

  // The remainder of |cards| were not listed in |changes_from_sync|.
  for (const auto& it : cards) {
    cache_.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                   it.first, *it.second));
  }

  syncer::SyncError status;
  if (!changes_to_sync.empty())
    status = SendChangesToSyncServer(changes_to_sync);

  return status;
}

void AutofillWalletMetadataSyncableService::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sync_processor_ && change.profile() &&
      change.profile()->record_type() == AutofillProfile::SERVER_PROFILE) {
    AutofillDataModelChanged(change.profile()->server_id(), *change.profile());
  }
}

void AutofillWalletMetadataSyncableService::CreditCardChanged(
    const CreditCardChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sync_processor_ && change.card() &&
      change.card()->record_type() != CreditCard::LOCAL_CARD) {
    AutofillDataModelChanged(change.card()->server_id(), *change.card());
  }
}

void AutofillWalletMetadataSyncableService::AutofillMultipleChanged() {
  // Merging data will clear the cache, so make a copy to avoid merging with
  // empty remote data. Copying the cache is expensive, but still cheaper than
  // GetAllSyncData().
  if (sync_processor_)
    MergeData(syncer::SyncDataList(cache_));
}

// static
void AutofillWalletMetadataSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutofillWalletMetadataSyncableService(webdata_backend, app_locale));
}

// static
AutofillWalletMetadataSyncableService*
AutofillWalletMetadataSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletMetadataSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

AutofillWalletMetadataSyncableService::AutofillWalletMetadataSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend), scoped_observer_(this) {
  // No webdata in tests.
  if (webdata_backend_)
    scoped_observer_.Add(webdata_backend_);
}

bool AutofillWalletMetadataSyncableService::GetLocalData(
    std::map<std::string, AutofillProfile*>* profiles,
    std::map<std::string, CreditCard*>* cards) const {
  std::vector<AutofillProfile*> profile_list;
  bool success = AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase())
                     ->GetServerProfiles(&profile_list);
  for (AutofillProfile* profile : profile_list)
    profiles->insert(std::make_pair(profile->server_id(), profile));

  std::vector<CreditCard*> card_list;
  success &= AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase())
                 ->GetServerCreditCards(&card_list);
  for (CreditCard* card : card_list)
    cards->insert(std::make_pair(card->server_id(), card));

  return success;
}

bool AutofillWalletMetadataSyncableService::UpdateAddressStats(
    const AutofillProfile& profile) {
  return AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase())
      ->UpdateServerAddressUsageStats(profile);
}

bool AutofillWalletMetadataSyncableService::UpdateCardStats(
    const CreditCard& credit_card) {
  return AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase())
      ->UpdateServerCardUsageStats(credit_card);
}

syncer::SyncError
AutofillWalletMetadataSyncableService::SendChangesToSyncServer(
    const syncer::SyncChangeList& changes_to_sync) {
  DCHECK(sync_processor_);
  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes_to_sync);
}

syncer::SyncMergeResult AutofillWalletMetadataSyncableService::MergeData(
    const syncer::SyncDataList& sync_data) {
  cache_.clear();

  std::map<std::string, AutofillProfile*> profiles;
  std::map<std::string, CreditCard*> cards;
  GetLocalData(&profiles, &cards);

  syncer::SyncMergeResult result(syncer::AUTOFILL_WALLET_METADATA);
  result.set_num_items_before_association(profiles.size() + cards.size());

  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncData& remote : sync_data) {
    DCHECK(remote.IsValid());
    DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, remote.GetDataType());
    switch (remote.GetSpecifics().wallet_metadata().type()) {
      case sync_pb::WalletMetadataSpecifics::ADDRESS:
        if (!MergeRemote(remote, address_updater, &profiles,
                         &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::CARD:
        if (!MergeRemote(remote, card_updater, &cards, &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  // The remainder of |profiles| were not listed in |sync_data|.
  for (const auto& it : profiles) {
    cache_.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::ADDRESS,
                                   it.first, *it.second));
    changes_to_sync.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD, cache_.back()));
  }

  // The remainder of |cards| were not listed in |sync_data|.
  for (const auto& it : cards) {
    cache_.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                   it.first, *it.second));
    changes_to_sync.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD, cache_.back()));
  }

  // Metadata is not added or deleted locally to maintain a 1:1 relationship
  // with Wallet data.
  result.set_num_items_after_association(result.num_items_before_association());
  result.set_num_items_added(0);
  result.set_num_items_deleted(0);

  if (!changes_to_sync.empty())
    result.set_error(SendChangesToSyncServer(changes_to_sync));

  return result;
}

template <class DataType>
bool AutofillWalletMetadataSyncableService::MergeRemote(
    const syncer::SyncData& remote,
    const base::Callback<bool(const DataType&)>& updater,
    std::map<std::string, DataType*>* locals,
    syncer::SyncChangeList* changes_to_sync) {
  DCHECK(locals);
  DCHECK(changes_to_sync);

  const sync_pb::WalletMetadataSpecifics& remote_metadata =
      remote.GetSpecifics().wallet_metadata();
  auto it = locals->find(remote_metadata.id());
  if (it == locals->end())
    return false;

  DataType* local_metadata = it->second;
  locals->erase(it);

  size_t remote_use_count = static_cast<size_t>(remote_metadata.use_count());
  bool is_local_modified = false;
  bool is_remote_outdated = false;
  if (local_metadata->use_count() < remote_use_count) {
    local_metadata->set_use_count(remote_use_count);
    is_local_modified = true;
  } else if (local_metadata->use_count() > remote_use_count) {
    is_remote_outdated = true;
  }

  base::Time remote_use_date =
      base::Time::FromInternalValue(remote_metadata.use_date());
  if (local_metadata->use_date() < remote_use_date) {
    local_metadata->set_use_date(remote_use_date);
    is_local_modified = true;
  } else if (local_metadata->use_date() > remote_use_date) {
    is_remote_outdated = true;
  }

  if (is_remote_outdated) {
    cache_.push_back(BuildSyncData(remote_metadata.type(), remote_metadata.id(),
                                   *local_metadata));
    changes_to_sync->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, cache_.back()));
  } else {
    cache_.push_back(remote);
  }

  if (is_local_modified)
    updater.Run(*local_metadata);

  return true;
}

void AutofillWalletMetadataSyncableService::AutofillDataModelChanged(
    const std::string& server_id,
    const AutofillDataModel& local) {
  for (auto it = cache_.begin(); it != cache_.end(); ++it) {
    const sync_pb::WalletMetadataSpecifics& remote =
        it->GetSpecifics().wallet_metadata();
    if (remote.id() == server_id) {
      if (static_cast<size_t>(remote.use_count()) < local.use_count() &&
          base::Time::FromInternalValue(remote.use_date()) < local.use_date()) {
        *it = BuildSyncData(remote.type(), server_id, local);
        SendChangesToSyncServer(syncer::SyncChangeList(
            1, syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                                  *it)));
      }

      return;
    }
  }
}

}  // namespace autofill
