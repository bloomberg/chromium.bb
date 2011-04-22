// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backend_migrator.h"

#include <algorithm>

#include "base/string_number_conversions.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

using syncable::ModelTypeSet;

namespace browser_sync {

using sessions::SyncSessionSnapshot;

BackendMigrator::BackendMigrator(ProfileSyncService* service,
                                 DataTypeManager* manager)
    : state_(IDLE), service_(service), manager_(manager),
      restart_migration_(false),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  registrar_.Add(this, NotificationType::SYNC_CONFIGURE_DONE,
                 Source<DataTypeManager>(manager_));
  service_->AddObserver(this);
}

BackendMigrator::~BackendMigrator() {
  service_->RemoveObserver(this);
}

bool BackendMigrator::HasStartedMigrating() const {
  return state_ >= DISABLING_TYPES;
}

void BackendMigrator::MigrateTypes(const syncable::ModelTypeSet& types) {
  {
    ModelTypeSet temp;
    std::set_union(to_migrate_.begin(), to_migrate_.end(),
                   types.begin(), types.end(),
                   std::inserter(temp, temp.end()));
    to_migrate_ = temp;
  }

  if (HasStartedMigrating()) {
    VLOG(1) << "BackendMigrator::MigrateTypes: STARTED_MIGRATING early-out.";
    restart_migration_ = true;
    return;
  }

  if (manager_->state() != DataTypeManager::CONFIGURED) {
    VLOG(1) << "BackendMigrator::MigrateTypes: manager CONFIGURED early-out.";
    state_ = WAITING_TO_START;
    return;
  }

  // We'll now disable any running types that need to be migrated.
  state_ = DISABLING_TYPES;
  ModelTypeSet full_set;
  service_->GetPreferredDataTypes(&full_set);
  ModelTypeSet difference;
  std::set_difference(full_set.begin(), full_set.end(),
                      to_migrate_.begin(), to_migrate_.end(),
                      std::inserter(difference, difference.end()));
  VLOG(1) << "BackendMigrator disabling types; calling Configure.";
  manager_->Configure(difference);
}

void BackendMigrator::OnStateChanged() {
  if (restart_migration_ == true) {
    VLOG(1) << "BackendMigrator restarting migration in OnStateChanged.";
    state_ = WAITING_TO_START;
    restart_migration_ = false;
    MigrateTypes(to_migrate_);
    return;
  }

  if (state_ != WAITING_FOR_PURGE)
    return;

  size_t num_empty_migrated_markers = 0;
  const SyncSessionSnapshot* snap = service_->GetLastSessionSnapshot();
  for (ModelTypeSet::const_iterator it = to_migrate_.begin();
       it != to_migrate_.end(); ++it) {
    if (snap->download_progress_markers[*it].empty())
      num_empty_migrated_markers++;
  }

  if (num_empty_migrated_markers < to_migrate_.size())
    return;

  state_ = REENABLING_TYPES;
  ModelTypeSet full_set;
  service_->GetPreferredDataTypes(&full_set);
  VLOG(1) << "BackendMigrator re-enabling types.";
  // Don't use |to_migrate_| for the re-enabling because the user may have
  // chosen to disable types during the migration.
  manager_->Configure(full_set);
}

void BackendMigrator::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::SYNC_CONFIGURE_DONE, type.value);
  if (state_ == IDLE)
    return;

  DataTypeManager::ConfigureResultWithErrorLocation* result =
      Details<DataTypeManager::ConfigureResultWithErrorLocation>(
          details).ptr();

  ModelTypeSet intersection;
  std::set_intersection(result->requested_types.begin(),
      result->requested_types.end(), to_migrate_.begin(), to_migrate_.end(),
      std::inserter(intersection, intersection.end()));

  // The intersection check is to determine if our disable request was
  // interrupted by a user changing preferred types.  May still need to purge.
  // It's pretty wild if we're in WAITING_FOR_PURGE here, because it would mean
  // that after our disable-config finished but before the purge, another config
  // was posted externally _and completed_, which means somehow the nudge to
  // purge was dropped, yet nudges are reliable.
  if (state_ == WAITING_TO_START || state_ == WAITING_FOR_PURGE ||
      (state_ == DISABLING_TYPES && !intersection.empty())) {
    state_ = WAITING_TO_START;
    restart_migration_ = false;
    VLOG(1) << "BackendMigrator::Observe posting MigrateTypes.";
    if (!BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        method_factory_.NewRunnableMethod(&BackendMigrator::MigrateTypes,
                                          to_migrate_))) {
      // Unittests need this.
      // TODO(tim): Clean this up.
      MigrateTypes(to_migrate_);
    }
    return;
  }

  if (result->result != DataTypeManager::OK) {
    // If this fails, and we're disabling types, a type may or may not be
    // disabled until the user restarts the browser.  If this wasn't an abort,
    // any failure will be reported as an unrecoverable error to the UI. If it
    // was an abort, then typically things are shutting down anyway. There isn't
    // much we can do in any case besides wait until a restart to try again.
    // The server will send down MIGRATION_DONE again for types needing
    // migration as the type will still be enabled on restart.
    LOG(WARNING) << "Unable to migrate, configuration failed!";
    state_ = IDLE;
    to_migrate_.clear();
    return;
  }

  if (state_ == DISABLING_TYPES) {
    state_ = WAITING_FOR_PURGE;
    VLOG(1) << "BackendMigrator waiting for purge.";
  } else if (state_ == REENABLING_TYPES) {
    // We're done!
    state_ = IDLE;

    std::stringstream ss;
    std::copy(to_migrate_.begin(), to_migrate_.end(),
              std::ostream_iterator<syncable::ModelType>(ss, ","));
    VLOG(1) << "BackendMigrator: Migration complete for: " << ss.str();
    to_migrate_.clear();
  }
}

BackendMigrator::State BackendMigrator::state() const {
  return state_;
}

};  // namespace browser_sync
