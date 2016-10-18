// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/sync_cycle.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/syncable/directory.h"

namespace syncer {

// static
SyncCycle* SyncCycle::Build(SyncCycleContext* context, Delegate* delegate) {
  return new SyncCycle(context, delegate);
}

SyncCycle::SyncCycle(SyncCycleContext* context, Delegate* delegate)
    : context_(context), delegate_(delegate) {
  status_controller_ = base::MakeUnique<StatusController>();
}

SyncCycle::~SyncCycle() {}

SyncCycleSnapshot SyncCycle::TakeSnapshot() const {
  return TakeSnapshotWithSource(sync_pb::GetUpdatesCallerInfo::UNKNOWN);
}

SyncCycleSnapshot SyncCycle::TakeSnapshotWithSource(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource legacy_updates_source)
    const {
  syncable::Directory* dir = context_->directory();

  ProgressMarkerMap download_progress_markers;
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType type(ModelTypeFromInt(i));
    dir->GetDownloadProgressAsString(type, &download_progress_markers[type]);
  }

  std::vector<int> num_entries_by_type(MODEL_TYPE_COUNT, 0);
  std::vector<int> num_to_delete_entries_by_type(MODEL_TYPE_COUNT, 0);
  dir->CollectMetaHandleCounts(&num_entries_by_type,
                               &num_to_delete_entries_by_type);

  SyncCycleSnapshot snapshot(
      status_controller_->model_neutral_state(), download_progress_markers,
      delegate_->IsCurrentlyThrottled(),
      status_controller_->num_encryption_conflicts(),
      status_controller_->num_hierarchy_conflicts(),
      status_controller_->num_server_conflicts(),
      context_->notifications_enabled(), dir->GetEntriesCount(),
      status_controller_->sync_start_time(),
      status_controller_->poll_finish_time(), num_entries_by_type,
      num_to_delete_entries_by_type, legacy_updates_source);

  return snapshot;
}

void SyncCycle::SendSyncCycleEndEventNotification(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  SyncCycleEvent event(SyncCycleEvent::SYNC_CYCLE_ENDED);
  event.snapshot = TakeSnapshotWithSource(source);

  DVLOG(1) << "Sending cycle end event with snapshot: "
           << event.snapshot.ToString();
  for (auto& observer : *context_->listeners())
    observer.OnSyncCycleEvent(event);
}

void SyncCycle::SendEventNotification(SyncCycleEvent::EventCause cause) {
  SyncCycleEvent event(cause);
  event.snapshot = TakeSnapshot();

  DVLOG(1) << "Sending event with snapshot: " << event.snapshot.ToString();
  for (auto& observer : *context_->listeners())
    observer.OnSyncCycleEvent(event);
}

void SyncCycle::SendProtocolEvent(const ProtocolEvent& event) {
  for (auto& observer : *context_->listeners())
    observer.OnProtocolEvent(event);
}

}  // namespace syncer
