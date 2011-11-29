// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backend_migrator.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using syncable::ModelTypeSet;

namespace browser_sync {

using sessions::SyncSessionSnapshot;
using syncable::ModelTypeToString;

MigrationObserver::~MigrationObserver() {}

BackendMigrator::BackendMigrator(const std::string& name,
                                 sync_api::UserShare* user_share,
                                 ProfileSyncService* service,
                                 DataTypeManager* manager)
    : name_(name), user_share_(user_share), service_(service),
      manager_(manager), state_(IDLE),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                 content::Source<DataTypeManager>(manager_));
}

BackendMigrator::~BackendMigrator() {
}

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

void BackendMigrator::MigrateTypes(const syncable::ModelTypeSet& types) {
  const ModelTypeSet old_to_migrate = to_migrate_;
  {
    ModelTypeSet temp;
    std::set_union(to_migrate_.begin(), to_migrate_.end(),
                   types.begin(), types.end(),
                   std::inserter(temp, temp.end()));
    std::swap(temp, to_migrate_);
  }
  SDVLOG(1) << "MigrateTypes called with " << ModelTypeSetToString(types)
            << ", old_to_migrate = " << ModelTypeSetToString(old_to_migrate)
            << ", to_migrate_ = " << ModelTypeSetToString(to_migrate_);
  if (old_to_migrate == to_migrate_) {
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
  if (manager_->state() == DataTypeManager::CONFIGURED) {
    RestartMigration();
    return true;
  }
  return false;
}

void BackendMigrator::RestartMigration() {
  // We'll now disable any running types that need to be migrated.
  ChangeState(DISABLING_TYPES);
  ModelTypeSet full_set;
  service_->GetPreferredDataTypes(&full_set);
  ModelTypeSet difference;
  std::set_difference(full_set.begin(), full_set.end(),
                      to_migrate_.begin(), to_migrate_.end(),
                      std::inserter(difference, difference.end()));
  bool configure_with_nigori = to_migrate_.count(syncable::NIGORI) == 0;
  SDVLOG(1) << "BackendMigrator disabling types "
            << ModelTypeSetToString(to_migrate_) << "; configuring "
            << ModelTypeSetToString(difference)
            << (configure_with_nigori ? " with nigori" : " without nigori");

  // Add nigori for config or not based upon if the server told us to migrate
  // nigori or not.
  if (configure_with_nigori) {
    manager_->Configure(difference, sync_api::CONFIGURE_REASON_MIGRATION);
  } else {
    manager_->ConfigureWithoutNigori(difference,
                                     sync_api::CONFIGURE_REASON_MIGRATION);
  }
}

void BackendMigrator::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE, type);
  if (state_ == IDLE)
    return;

  // |manager_|'s methods aren't re-entrant, and we're notified from
  // them, so post a task to avoid problems.
  SDVLOG(1) << "Posting OnConfigureDone from Observer";
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BackendMigrator::OnConfigureDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 *content::Details<DataTypeManager::ConfigureResult>(
                     details).ptr()));
}

namespace {

syncable::ModelTypeSet GetUnsyncedDataTypes(sync_api::UserShare* user_share) {
  sync_api::ReadTransaction trans(FROM_HERE, user_share);
  syncable::ModelTypeSet unsynced_data_types;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type = syncable::ModelTypeFromInt(i);
    sync_pb::DataTypeProgressMarker progress_marker;
    trans.GetLookup()->GetDownloadProgress(type, &progress_marker);
    if (progress_marker.token().empty()) {
      unsynced_data_types.insert(type);
    }
  }
  return unsynced_data_types;
}

}  // namespace

void BackendMigrator::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
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

  ModelTypeSet intersection;
  std::set_intersection(
      result.requested_types.begin(), result.requested_types.end(),
      to_migrate_.begin(), to_migrate_.end(),
      std::inserter(intersection, intersection.end()));
  // This intersection check is to determine if our disable request
  // was interrupted by a user changing preferred types.
  if (state_ == DISABLING_TYPES && !intersection.empty()) {
    SDVLOG(1) << "Disable request interrupted by user changing types";
    RestartMigration();
    return;
  }

  if (result.status != DataTypeManager::OK) {
    // If this fails, and we're disabling types, a type may or may not be
    // disabled until the user restarts the browser.  If this wasn't an abort,
    // any failure will be reported as an unrecoverable error to the UI. If it
    // was an abort, then typically things are shutting down anyway. There isn't
    // much we can do in any case besides wait until a restart to try again.
    // The server will send down MIGRATION_DONE again for types needing
    // migration as the type will still be enabled on restart.
    SLOG(WARNING) << "Unable to migrate, configuration failed!";
    ChangeState(IDLE);
    to_migrate_.clear();
    return;
  }

  if (state_ == DISABLING_TYPES) {
    const syncable::ModelTypeSet& unsynced_types =
        GetUnsyncedDataTypes(user_share_);
    if (!std::includes(unsynced_types.begin(), unsynced_types.end(),
                       to_migrate_.begin(), to_migrate_.end())) {
      SLOG(WARNING) << "Set of unsynced types: "
                    << syncable::ModelTypeSetToString(unsynced_types)
                    << " does not contain types to migrate: "
                    << syncable::ModelTypeSetToString(to_migrate_)
                    << "; not re-enabling yet";
      return;
    }

    ChangeState(REENABLING_TYPES);
    // Don't use |to_migrate_| for the re-enabling because the user
    // may have chosen to disable types during the migration.
    ModelTypeSet full_set;
    service_->GetPreferredDataTypes(&full_set);
    SDVLOG(1) << "BackendMigrator re-enabling types: "
              << syncable::ModelTypeSetToString(full_set);
    manager_->Configure(full_set, sync_api::CONFIGURE_REASON_MIGRATION);
  } else if (state_ == REENABLING_TYPES) {
    // We're done!
    ChangeState(IDLE);

    SDVLOG(1) << "BackendMigrator: Migration complete for: "
              << syncable::ModelTypeSetToString(to_migrate_);
    to_migrate_.clear();
  }
}

BackendMigrator::State BackendMigrator::state() const {
  return state_;
}

syncable::ModelTypeSet
    BackendMigrator::GetPendingMigrationTypesForTest() const {
  return to_migrate_;
}

#undef SDVLOG

#undef SLOG

};  // namespace browser_sync
