// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/threading/non_thread_safe.h"
#include "components/webdata/autofill/autofill_change.h"
#include "components/webdata/autofill/autofill_entry.h"
#include "components/webdata/autofill/autofill_webdata_service.h"
#include "components/webdata/autofill/autofill_webdata_service_observer.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

class ProfileSyncServiceAutofillTest;

namespace syncer {
class SyncErrorFactory;
}

namespace sync_pb {
class AutofillSpecifics;
}

// The sync implementation for autocomplete.
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
class AutocompleteSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public AutofillWebDataServiceObserverOnDBThread,
      public base::NonThreadSafe {
 public:
  virtual ~AutocompleteSyncableService();

  // Creates a new AutocompleteSyncableService and hangs it off of
  // |web_data_service|, which takes ownership.
  static void CreateForWebDataService(AutofillWebDataService* web_data_service);
  // Retrieves the AutocompleteSyncableService stored on |web_data|.
  static AutocompleteSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

  static syncer::ModelType model_type() { return syncer::AUTOFILL; }

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // AutofillWebDataServiceObserverOnDBThread implementation.
  virtual void AutofillEntriesChanged(
      const AutofillChangeList& changes) OVERRIDE;

  // Called via sync to tell us if we should cull expired entries when merging
  // and/or processing sync changes.
  void UpdateCullSetting(bool cull_expired_entries);
  bool cull_expired_entries() const { return cull_expired_entries_; }

 protected:
  explicit AutocompleteSyncableService(
      AutofillWebDataService* web_data_service);

  // Helper to query WebDatabase for the current autocomplete state.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool LoadAutofillData(std::vector<AutofillEntry>* entries) const;

  // Helper to persist any changes that occured during model association to
  // the WebDatabase. |entries| will be either added or updated.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool SaveChangesToWebData(const std::vector<AutofillEntry>& entries);

 private:
  friend class ProfileSyncServiceAutofillTest;
  friend class MockAutocompleteSyncableService;
  friend class FakeServerUpdater;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteSyncableServiceTest,
                           MergeDataAndStartSyncing);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteSyncableServiceTest, GetAllSyncData);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteSyncableServiceTest,
                           ProcessSyncChanges);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteSyncableServiceTest,
                           ActOnChange);

  // This is a helper map used only in Merge/Process* functions. The lifetime
  // of the iterator is longer than the map object. The bool in the pair is used
  // to indicate if the item needs to be added (true) or updated (false).
  typedef std::map<AutofillKey,
                  std::pair<syncer::SyncChange::SyncChangeType,
                            std::vector<AutofillEntry>::iterator> >
      AutocompleteEntryMap;

  // Creates or updates an autocomplete entry based on |data|.
  // |data| -  an entry for sync.
  // |loaded_data| - entries that were loaded from local storage.
  // |new_entries| - entries that came from the sync.
  // |ignored_entries| - entries that came from the sync, but too old to be
  // stored and immediately discarded.
  void CreateOrUpdateEntry(const syncer::SyncData& data,
                           AutocompleteEntryMap* loaded_data,
                           std::vector<AutofillEntry>* new_entries);

  // Writes |entry| data into supplied |autofill_specifics|.
  static void WriteAutofillEntry(const AutofillEntry& entry,
                                 sync_pb::EntitySpecifics* autofill_specifics);

  // Deletes the database entry corresponding to the |autofill| specifics.
  syncer::SyncError AutofillEntryDelete(
      const sync_pb::AutofillSpecifics& autofill);

  syncer::SyncData CreateSyncData(const AutofillEntry& entry) const;

  // Syncs |changes| to the cloud.
  void ActOnChanges(const AutofillChangeList& changes);

  static std::string KeyToTag(const std::string& name,
                              const std::string& value);

  // For unit-tests.
  AutocompleteSyncableService();
  void set_sync_processor(syncer::SyncChangeProcessor* sync_processor) {
    sync_processor_.reset(sync_processor);
  }

  // Lifetime of AutocompleteSyncableService object is shorter than
  // |web_data_service_| passed to it.
  AutofillWebDataService* web_data_service_;

  ScopedObserver<AutofillWebDataService, AutocompleteSyncableService>
      scoped_observer_;

  // We receive ownership of |sync_processor_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // We receive ownership of |error_handler_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  scoped_ptr<syncer::SyncErrorFactory> error_handler_;

  // Whether we should cull expired autofill entries, can be updated by sync
  // via UpdateCullingSetting.
  bool cull_expired_entries_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteSyncableService);
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
