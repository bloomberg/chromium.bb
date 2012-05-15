// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

class ProfileSyncServiceAutofillTest;
class SyncErrorFactory;

namespace sync_pb {
class AutofillSpecifics;
}


// The sync implementation for autocomplete.
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
// TODO(georgey) : remove reliance on the notifications and make it to be called
// from web_data_service directly.
class AutocompleteSyncableService
    : public SyncableService,
      public content::NotificationObserver,
      public base::NonThreadSafe {
 public:
  explicit AutocompleteSyncableService(WebDataService* web_data_service);
  virtual ~AutocompleteSyncableService();

  static syncable::ModelType model_type() { return syncable::AUTOFILL; }

  // SyncableService implementation.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
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
                  std::pair<SyncChange::SyncChangeType,
                            std::vector<AutofillEntry>::iterator> >
      AutocompleteEntryMap;

  // Creates or updates an autocomplete entry based on |data|.
  // |data| -  an entry for sync.
  // |loaded_data| - entries that were loaded from local storage.
  // |new_entries| - entries that came from the sync.
  // |ignored_entries| - entries that came from the sync, but too old to be
  // stored and immediately discarded.
  void CreateOrUpdateEntry(const SyncData& data,
                           AutocompleteEntryMap* loaded_data,
                           std::vector<AutofillEntry>* new_entries,
                           std::vector<AutofillEntry>* ignored_entries);

  // Writes |entry| data into supplied |autofill_specifics|.
  static void WriteAutofillEntry(const AutofillEntry& entry,
                                 sync_pb::EntitySpecifics* autofill_specifics);

  // Deletes the database entry corresponding to the |autofill| specifics.
  SyncError AutofillEntryDelete(const sync_pb::AutofillSpecifics& autofill);

  SyncData CreateSyncData(const AutofillEntry& entry) const;

  // Syncs |changes| to the cloud.
  void ActOnChanges(const AutofillChangeList& changes);

  static std::string KeyToTag(const std::string& name,
                              const std::string& value);

  // For unit-tests.
  AutocompleteSyncableService();
  void set_sync_processor(SyncChangeProcessor* sync_processor) {
    sync_processor_.reset(sync_processor);
  }

  // Ignore deletions of the following keys as they were never synced.
  std::set<AutofillKey> keys_to_ignore_;

  // Lifetime of AutocompleteSyncableService object is shorter than
  // |web_data_service_| passed to it.
  WebDataService* web_data_service_;
  content::NotificationRegistrar notification_registrar_;

  // We receive ownership of |sync_processor_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  scoped_ptr<SyncChangeProcessor> sync_processor_;

  // We receive ownership of |error_handler_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  scoped_ptr<SyncErrorFactory> error_handler_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteSyncableService);
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
