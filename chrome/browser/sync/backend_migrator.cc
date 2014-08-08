// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backend_migrator.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h" // TODO(tim): Bug 131130.

using syncer::ModelTypeSet;

namespace browser_sync {

using syncer::ModelTypeToString;

MigrationObserver::~MigrationObserver() {}

BackendMigrator::BackendMigrator(const std::string& name,
                                 syncer::UserShare* user_share,
                                 ProfileSyncService* service,
                                 sync_driver::DataTypeManager* manager,
                                 const base::Closure &migration_done_callback)
    : name_(name), user_share_(user_share), service_(service),
      manager_(manager), state_(IDLE),
      migration_done_callback_(migration_done_callback),
      weak_ptr_factory_(this) {
}

BackendMigrator::~BackendMigrator() {
}

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

void BackendMigrator::MigrateTypes(syncer::ModelTypeSet types) {
  const ModelTypeSet old_to_migrate = to_migrate_;
  to_migrate_.PutAll(types);
  SDVLOG(1) << "MigrateTypes called with " << ModelTypeSetToString(types)
           << ", old_to_migrate = " << ModelTypeSetToString(old_to_migrate)
          << ", to_migrate_ = " << ModelTypeSetToString(to_migrate_);
  if (old_to_migrate.Equals(to_migrate_)) {
    SDVLOG(1) << "MigrateTypes called with no new types; ignoring";
    return;
  }

  if (state_ == IDLE)
    ChangeState(WAITING_TO_START);

  if (state_ == WAITING_TO_START) {
    if (!TryStart())
      SDVLOG(1) << "Manager not configured; waiting";
    return;
  }

  DCHECK_GT(state_, WAITING_TO_START);
  // If we're already migrating, interrupt the current migration.
  RestartMigration();
}

void BackendMigrator::AddMigrationObserver(MigrationObserver* observer) {
  migration_observers_.AddObserver(observer);
}

bool BackendMigrator::HasMigrationObserver(
    MigrationObserver* observer) const {
  return migration_observers_.HasObserver(observer);
}

void BackendMigrator::RemoveMigrationObserver(MigrationObserver* observer) {
  migration_observers_.RemoveObserver(observer);
}

void BackendMigrator::ChangeState(State new_state) {
  state_ = new_state;
  FOR_EACH_OBSERVER(MigrationObserver, migration_observers_,
                    OnMigrationStateChange());
}

bool BackendMigrator::TryStart() {
  DCHECK_EQ(state_, WAITING_TO_START);
  if (manager_->state() == sync_driver::DataTypeManager::CONFIGURED) {
    RestartMigration();
    return true;
  }
  return false;
}

void BackendMigrator::RestartMigration() {
  // We'll now disable any running types that need to be migrated.
  ChangeState(DISABLING_TYPES);
  SDVLOG(1) << "BackendMigrator disabling types "
            << ModelTypeSetToString(to_migrate_);

  manager_->PurgeForMigration(to_migrate_, syncer::CONFIGURE_REASON_MIGRATION);
}

void BackendMigrator::OnConfigureDone(
    const sync_driver::DataTypeManager::ConfigureResult& result) {
  if (state_ == IDLE)
    return;

  // |manager_|'s methods aren't re-entrant, and we're notified from
  // them, so post a task to avoid problems.
  SDVLOG(1) << "Posting OnConfigureDoneImpl";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BackendMigrator::OnConfigureDoneImpl,
                 weak_ptr_factory_.GetWeakPtr(), result));
}

namespace {

syncer::ModelTypeSet GetUnsyncedDataTypes(syncer::UserShare* user_share) {
  syncer::ReadTransaction trans(FROM_HERE, user_share);
  syncer::ModelTypeSet unsynced_data_types;
  for (int i = syncer::FIRST_REAL_MODEL_TYPE;
       i < syncer::MODEL_TYPE_COUNT; ++i) {
    syncer::ModelType type = syncer::ModelTypeFromInt(i);
    sync_pb::DataTypeProgressMarker progress_marker;
    trans.GetDirectory()->GetDownloadProgress(type, &progress_marker);
    if (progress_marker.token().empty()) {
      unsynced_data_types.Put(type);
    }
  }
  return unsynced_data_types;
}

}  // namespace

void BackendMigrator::OnConfigureDoneImpl(
    const sync_driver::DataTypeManager::ConfigureResult& result) {
  SDVLOG(1) << "OnConfigureDone with requested types "
            << ModelTypeSetToString(result.requested_types)
            << ", status " << result.status
            << ", and to_migrate_ = " << ModelTypeSetToString(to_migrate_);
  if (state_ == WAITING_TO_START) {
    if (!TryStart())
      SDVLOG(1) << "Manager still not configured; still waiting";
    return;
  }

  DCHECK_GT(state_, WAITING_TO_START);

  const ModelTypeSet intersection =
      Intersection(result.requested_types, to_migrate_);
  // This intersection check is to determine if our disable request
  // was interrupted by a user changing preferred types.
  if (state_ == DISABLING_TYPES && !intersection.Empty()) {
    SDVLOG(1) << "Disable request interrupted by user changing types";
    RestartMigration();
    return;
  }

  if (result.status != sync_driver::DataTypeManager::OK) {
    // If this fails, and we're disabling types, a type may or may not be
    // disabled until the user restarts the browser.  If this wasn't an abort,
    // any failure will be reported as an unrecoverable error to the UI. If it
    // was an abort, then typically things are shutting down anyway. There isn't
    // much we can do in any case besides wait until a restart to try again.
    // The server will send down MIGRATION_DONE again for types needing
    // migration as the type will still be enabled on restart.
    SLOG(WARNING) << "Unable to migrate, configuration failed!";
    ChangeState(IDLE);
    to_migrate_.Clear();
    return;
  }

  if (state_ == DISABLING_TYPES) {
    const syncer::ModelTypeSet unsynced_types =
        GetUnsyncedDataTypes(user_share_);
    if (!unsynced_types.HasAll(to_migrate_)) {
      SLOG(WARNING) << "Set of unsynced types: "
                    << syncer::ModelTypeSetToString(unsynced_types)
                    << " does not contain types to migrate: "
                    << syncer::ModelTypeSetToString(to_migrate_)
                    << "; not re-enabling yet";
      return;
    }

    ChangeState(REENABLING_TYPES);
    // Don't use |to_migrate_| for the re-enabling because the user
    // may have chosen to disable types during the migration.
    const ModelTypeSet full_set = service_->GetPreferredDataTypes();
    SDVLOG(1) << "BackendMigrator re-enabling types: "
              << syncer::ModelTypeSetToString(full_set);
    manager_->Configure(full_set, syncer::CONFIGURE_REASON_MIGRATION);
  } else if (state_ == REENABLING_TYPES) {
    // We're done!
    ChangeState(IDLE);

    SDVLOG(1) << "BackendMigrator: Migration complete for: "
              << syncer::ModelTypeSetToString(to_migrate_);
    to_migrate_.Clear();

    if (!migration_done_callback_.is_null())
      migration_done_callback_.Run();
  }
}

BackendMigrator::State BackendMigrator::state() const {
  return state_;
}

syncer::ModelTypeSet BackendMigrator::GetPendingMigrationTypesForTest() const {
  return to_migrate_;
}

#undef SDVLOG

#undef SLOG

};  // namespace browser_sync
