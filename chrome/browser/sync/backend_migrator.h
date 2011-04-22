// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
#define CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_

#include "base/task.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ProfileSyncService;

namespace browser_sync {

class DataTypeManager;

// A class to perform migration of a datatype pursuant to the 'MIGRATION_DONE'
// code in the sync protocol definition (protocol/sync.proto).
class BackendMigrator : public NotificationObserver,
                        public ProfileSyncServiceObserver {
 public:
  enum State {
    IDLE,
    WAITING_TO_START,   // Waiting for previous configuration to finish.
    DISABLING_TYPES,    // Exit criteria: SYNC_CONFIGURE_DONE for enabled
                        // types _excluding_ |to_migrate_|.
    WAITING_FOR_PURGE,  // Exit criteria: SyncCycleEnded for enabled types
                        // excluding |to_migrate|
    REENABLING_TYPES,   // Exit criteria: SYNC_CONFIGURE_DONE for enabled
                        // types.
  };

  BackendMigrator(ProfileSyncService* service, DataTypeManager* manager);
  virtual ~BackendMigrator();

  // Starts a sequence of events that will disable and reenable |types|.
  void MigrateTypes(const syncable::ModelTypeSet& types);

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  State state() const;

 private:
  bool HasStartedMigrating() const;

  State state_;
  ProfileSyncService* service_;
  DataTypeManager* manager_;
  NotificationRegistrar registrar_;

  syncable::ModelTypeSet to_migrate_;
  bool restart_migration_;

  // We use this to gracefully re-start migrations.
  ScopedRunnableMethodFactory<BackendMigrator> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackendMigrator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_BACKEND_MIGRATOR_H_
