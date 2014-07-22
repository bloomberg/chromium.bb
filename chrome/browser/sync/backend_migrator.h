// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
#define CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync_driver/data_type_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/internal_api/public/base/model_type.h"

class ProfileSyncService;

namespace syncer {
struct UserShare;
}  // namespace syncer

namespace browser_sync {

// Interface for anything that wants to know when the migrator's state
// changes.
class MigrationObserver {
 public:
  virtual void OnMigrationStateChange() = 0;

 protected:
  virtual ~MigrationObserver();
};

// A class to perform migration of a datatype pursuant to the 'MIGRATION_DONE'
// code in the sync protocol definition (protocol/sync.proto).
class BackendMigrator {
 public:
  enum State {
    IDLE,
    WAITING_TO_START,   // Waiting for previous configuration to finish.
    DISABLING_TYPES,    // Exit criteria: SYNC_CONFIGURE_DONE for
                        // enabled types _excluding_ |to_migrate_| and
                        // empty download progress markers for types
                        // in |to_migrate_|.
    REENABLING_TYPES,   // Exit criteria: SYNC_CONFIGURE_DONE for enabled
                        // types.
  };

  // TODO(akalin): Remove the dependency on |user_share|.
  BackendMigrator(const std::string& name,
                  syncer::UserShare* user_share,
                  ProfileSyncService* service,
                  sync_driver::DataTypeManager* manager,
                  const base::Closure &migration_done_callback);
  virtual ~BackendMigrator();

  // Starts a sequence of events that will disable and reenable |types|.
  void MigrateTypes(syncer::ModelTypeSet types);

  void AddMigrationObserver(MigrationObserver* observer);
  bool HasMigrationObserver(MigrationObserver* observer) const;
  void RemoveMigrationObserver(MigrationObserver* observer);

  State state() const;

  // Called from ProfileSyncService to notify us of configure done.
  // Note: We receive these notificiations only when our state is not IDLE.
  void OnConfigureDone(
      const sync_driver::DataTypeManager::ConfigureResult& result);

  // Returns the types that are currently pending migration (if any).
  syncer::ModelTypeSet GetPendingMigrationTypesForTest() const;

 private:
  void ChangeState(State new_state);

  // Must be called only in state WAITING_TO_START.  If ready to
  // start, meaning the data type manager is configured, calls
  // RestartMigration() and returns true.  Otherwise, does nothing and
  // returns false.
  bool TryStart();

  // Restarts migration, interrupting any existing migration.
  void RestartMigration();

  // Called by OnConfigureDone().
  void OnConfigureDoneImpl(
      const sync_driver::DataTypeManager::ConfigureResult& result);

  const std::string name_;
  syncer::UserShare* user_share_;
  ProfileSyncService* service_;
  sync_driver::DataTypeManager* manager_;

  State state_;

  ObserverList<MigrationObserver> migration_observers_;

  syncer::ModelTypeSet to_migrate_;

  base::Closure migration_done_callback_;

  base::WeakPtrFactory<BackendMigrator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackendMigrator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
