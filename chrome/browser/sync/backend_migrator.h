// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
#define CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ProfileSyncService;

namespace sync_api {
struct UserShare;
}  // namespace sync_api

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
class BackendMigrator : public NotificationObserver {
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
                  sync_api::UserShare* user_share,
                  ProfileSyncService* service,
                  DataTypeManager* manager);
  virtual ~BackendMigrator();

  // Starts a sequence of events that will disable and reenable |types|.
  void MigrateTypes(const syncable::ModelTypeSet& types);

  void AddMigrationObserver(MigrationObserver* observer);
  bool HasMigrationObserver(MigrationObserver* observer) const;
  void RemoveMigrationObserver(MigrationObserver* observer);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  State state() const;

  // Returns the types that are currently pending migration (if any).
  syncable::ModelTypeSet GetPendingMigrationTypesForTest() const;

 private:
  void ChangeState(State new_state);

  // Must be called only in state WAITING_TO_START.  If ready to
  // start, meaning the data type manager is configured, calls
  // RestartMigration() and returns true.  Otherwise, does nothing and
  // returns false.
  bool TryStart();

  // Restarts migration, interrupting any existing migration.
  void RestartMigration();

  // Called by Observe().
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result);

  const std::string name_;
  sync_api::UserShare* user_share_;
  ProfileSyncService* service_;
  DataTypeManager* manager_;

  State state_;

  NotificationRegistrar registrar_;

  ObserverList<MigrationObserver> migration_observers_;

  syncable::ModelTypeSet to_migrate_;

  base::WeakPtrFactory<BackendMigrator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackendMigrator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
