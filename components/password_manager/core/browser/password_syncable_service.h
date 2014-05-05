// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNCABLE_SERVICE_H__
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNCABLE_SERVICE_H__

#if !defined(PASSWORD_MANAGER_ENABLE_SYNC)
#error "Only build this file when sync is enabled in Password Manager."
#endif

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

namespace autofill {
struct PasswordForm;
}

namespace syncer {
class SyncErrorFactory;
}

namespace password_manager {

class PasswordStoreSync;

class PasswordSyncableService : public syncer::SyncableService,
                                public base::NonThreadSafe {
 public:
  // |PasswordSyncableService| is owned by |PasswordStore|.
  explicit PasswordSyncableService(PasswordStoreSync* password_store);
  virtual ~PasswordSyncableService();

  // syncer::SyncableServiceImplementations
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

  // Notifies sync of changes to the password database.
  void ActOnPasswordStoreChanges(const PasswordStoreChangeList& changes);

  // Provides a StartSyncFlare to the SyncableService. See
  // sync_start_util for more.
  void InjectStartSyncFlare(
      const syncer::SyncableService::StartSyncFlare& flare);

 private:
  typedef std::vector<autofill::PasswordForm*> PasswordForms;
  // Map from password sync tag to password form.
  typedef std::map<std::string, autofill::PasswordForm*> PasswordEntryMap;

  // Helper function to retrieve the entries from password db and fill both
  // |password_entries| and |passwords_entry_map|. |passwords_entry_map| can be
  // NULL.
  bool ReadFromPasswordStore(
      ScopedVector<autofill::PasswordForm>* password_entries,
      PasswordEntryMap* passwords_entry_map) const;

  // Uses the |PasswordStore| APIs to change entries.
  void WriteToPasswordStore(const PasswordForms& new_entries,
                            const PasswordForms& updated_entries,
                            const PasswordForms& deleted_entries);

  // Notifies password store of a change that was performed by sync.
  // Virtual so tests can override.
  virtual void NotifyPasswordStoreOfLoginChanges(
      const PasswordStoreChangeList& changes);

  // Checks if |data|, the entry in sync db, needs to be created or updated
  // in the passwords db. Depending on what action needs to be performed, the
  // entry may be added to |new_sync_entries| or to |updated_sync_entries|. If
  // the item is identical to an entry in the passwords db, no action is
  // performed. If an item needs to be updated in the sync db, then the item is
  // also added to |updated_db_entries| list. If |data|'s tag is identical to
  // an entry's tag in |umatched_data_from_password_db| then that entry will be
  // removed from |umatched_data_from_password_db|.
  void CreateOrUpdateEntry(
      const syncer::SyncData& data,
      PasswordEntryMap* umatched_data_from_password_db,
      ScopedVector<autofill::PasswordForm>* new_sync_entries,
      ScopedVector<autofill::PasswordForm>* updated_sync_entries,
      syncer::SyncChangeList* updated_db_entries);

  // The factory that creates sync errors. |SyncError| has rich data
  // suitable for debugging.
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  // |SyncProcessor| will mirror the |PasswordStore| changes in the sync db.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // The password store that adds/updates/deletes password entries.
  PasswordStoreSync* const password_store_;

  // A signal to start sync as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  // True if processing sync changes is in progress.
  bool is_processing_sync_changes_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncableService);
};

// Converts the |password| into a SyncData object.
syncer::SyncData SyncDataFromPassword(const autofill::PasswordForm& password);

// Extracts the |PasswordForm| data from sync's protobuffer format.
void PasswordFromSpecifics(const sync_pb::PasswordSpecificsData& password,
                           autofill::PasswordForm* new_password);

// Returns the unique tag that will serve as the sync identifier for the
// |password| entry.
std::string MakePasswordSyncTag(const sync_pb::PasswordSpecificsData& password);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNCABLE_SERVICE_H__
