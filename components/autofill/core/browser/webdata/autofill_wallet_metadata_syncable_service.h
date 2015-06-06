// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/syncable_service.h"

namespace syncer {
class SyncErrorFactory;
}

namespace tracked_objects {
class Location;
}

namespace autofill {

class AutofillDataModel;
class AutofillProfile;
class AutofillProfileChange;
class AutofillWebDataBackend;
class AutofillWebDataService;
class CreditCard;
class CreditCardChange;

// Syncs usage counts and last use dates (metadata) for Wallet cards and
// addresses (data). Creation and deletion of metadata on the sync server
// follows the creation and deletion of the local metadata. (Local metadata has
// 1:1 relationship with Wallet data.)
class AutofillWalletMetadataSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public AutofillWebDataServiceObserverOnDBThread {
 public:
  ~AutofillWalletMetadataSyncableService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& changes_from_sync) override;

  // AutofillWebDataServiceObserverOnDBThread implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;
  void CreditCardChanged(const CreditCardChange& change) override;
  void AutofillMultipleChanged() override;

  // Creates a new AutofillWalletMetadataSyncableService and hangs it off of
  // |web_data_service|, which takes ownership. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* webdata_backend,
      const std::string& app_locale);

  // Retrieves the AutofillWalletMetadataSyncableService stored on
  // |web_data_service|.
  static AutofillWalletMetadataSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

 protected:
  AutofillWalletMetadataSyncableService(AutofillWebDataBackend* webdata_backend,
                                        const std::string& app_locale);

  // Populates the provided |profiles| and |cards| with mappings from server ID
  // to unowned server profiles and server cards as read from disk. This data
  // contains the usage stats. Returns true on success.
  virtual bool GetLocalData(std::map<std::string, AutofillProfile*>* profiles,
                            std::map<std::string, CreditCard*>* cards) const;

  // Updates the stats for |profile| stored on disk. Does not trigger
  // notifications that this profile was updated.
  virtual bool UpdateAddressStats(const AutofillProfile& profile);

  // Updates the stats for |credit_card| stored on disk. Does not trigger
  // notifications that this credit card was updated.
  virtual bool UpdateCardStats(const CreditCard& credit_card);

  // Sends the |changes_to_sync| to the sync server.
  virtual syncer::SyncError SendChangesToSyncServer(
      const syncer::SyncChangeList& changes_to_sync);

 private:
  // Merges local metadata with |sync_data|.
  //
  // Sends an "update" to the sync server if |sync_data| contains metadata that
  // is present locally, but local metadata has higher use count and later use
  // date.
  //
  // Sends a "create" to the sync server if |sync_data| does not have some local
  // metadata.
  //
  // Sends a "delete" to the sync server if |sync_data| contains metadata that
  // is not present locally.
  syncer::SyncMergeResult MergeData(const syncer::SyncDataList& sync_data);

  // Merges |remote| metadata into a collection of metadata |locals|. Returns
  // true if the corresponding local metadata was found.
  //
  // Stores an "update" in |changes_to_sync| if |remote| corresponds to an item
  // in |locals| that has higher use count and later use date.
  template <class DataType>
  bool MergeRemote(const syncer::SyncData& remote,
                   const base::Callback<bool(const DataType&)>& updater,
                   std::map<std::string, DataType*>* locals,
                   syncer::SyncChangeList* changes_to_sync);

  // Sends updates to the sync server.
  void AutofillDataModelChanged(const std::string& server_id,
                                const AutofillDataModel& local);

  base::ThreadChecker thread_checker_;
  AutofillWebDataBackend* webdata_backend_;  // Weak ref.
  ScopedObserver<AutofillWebDataBackend, AutofillWalletMetadataSyncableService>
      scoped_observer_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  // Cache of sync server data maintained to avoid expensive calls to
  // GetAllSyncData().
  syncer::SyncDataList cache_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncableService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNCABLE_SERVICE_H_
