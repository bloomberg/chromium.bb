// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_TRACKER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_TRACKER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/base/invalidation_interface.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class InvalidationInterface;

struct WaitInterval {
  enum BlockingMode {
    // Uninitialized state, should not be set in practice.
    UNKNOWN = -1,
    // We enter a series of increasingly longer WaitIntervals if we experience
    // repeated transient failures.  We retry at the end of each interval.
    EXPONENTIAL_BACKOFF,
    // A server-initiated throttled interval.  We do not allow any syncing
    // during such an interval.
    THROTTLED,
    // We re retrying for exponetial backoff.
    EXPONENTIAL_BACKOFF_RETRYING,
  };
  WaitInterval();
  WaitInterval(BlockingMode mode, base::TimeDelta length);
  ~WaitInterval();

  static const char* GetModeString(BlockingMode mode);

  BlockingMode mode;
  base::TimeDelta length;
};

// A class to track the per-type scheduling data.
class DataTypeTracker {
 public:
  DataTypeTracker();
  ~DataTypeTracker();

  // For STL compatibility, we do not forbid the creation of a default copy
  // constructor and assignment operator.

  // Tracks that a local change has been made to this type.
  // Returns the current local change nudge delay for this type.
  base::TimeDelta RecordLocalChange();

  // Tracks that a local refresh request has been made for this type.
  void RecordLocalRefreshRequest();

  // Tracks that we received invalidation notifications for this type.
  void RecordRemoteInvalidation(
      std::unique_ptr<InvalidationInterface> incoming);

  // Takes note that initial sync is pending for this type.
  void RecordInitialSyncRequired();

  // Takes note that the conflict happended for this type, need to sync to
  // resolve conflict locally.
  void RecordCommitConflict();

  // Records that a sync cycle has been performed successfully.
  // Generally, this means that all local changes have been committed and all
  // remote changes have been downloaded, so we can clear any flags related to
  // pending work.
  // But if partial throttling and backoff happen, this function also will be
  // called since we count those cases as success. So we need to check if the
  // datatype is in partial throttling or backoff in the beginning of this
  // function.
  void RecordSuccessfulSyncCycle();

  // Records that the initial sync has completed successfully. This gets called
  // when the initial configuration/download cycle has finished for this type.
  void RecordInitialSyncDone();

  // Updates the size of the invalidations payload buffer.
  void UpdatePayloadBufferSize(size_t new_size);

  // Returns true if there is a good reason to perform a sync cycle.  This does
  // not take into account whether or not now is a good time to perform a sync
  // cycle.  That's for the scheduler to decide.
  bool IsSyncRequired() const;

  // Returns true if there is a good reason to fetch updates for this type as
  // part of the next sync cycle.
  bool IsGetUpdatesRequired() const;

  // Returns true if there is an uncommitted local change.
  bool HasLocalChangePending() const;

  // Returns true if we've received an invalidation since we last fetched
  // updates.
  bool HasPendingInvalidation() const;

  // Returns true if an explicit refresh request is still outstanding.
  bool HasRefreshRequestPending() const;

  // Returns true if this type is requesting an initial sync.
  bool IsInitialSyncRequired() const;

  // Returns true if this type is requesting a sync to resolve conflict issue.
  bool IsSyncRequiredToResolveConflict() const;

  // Fills in the legacy invalidaiton payload information fields.
  void SetLegacyNotificationHint(
      sync_pb::DataTypeProgressMarker* progress) const;

  // Fills some type-specific contents of a GetUpdates request protobuf.  These
  // messages provide the server with the information it needs to decide how to
  // handle a request.
  void FillGetUpdatesTriggersMessage(sync_pb::GetUpdateTriggers* msg) const;

  // Returns true if the type is currently throttled or backed off.
  bool IsBlocked() const;

  // Returns the time until this type's throttling or backoff interval expires.
  // Should not be called unless IsThrottled() or IsBackedOff() returns true.
  // The returned value will be increased to zero if it would otherwise have
  // been negative.
  base::TimeDelta GetTimeUntilUnblock() const;

  // Returns the last backoff interval.
  base::TimeDelta GetLastBackoffInterval() const;

  // Throttles the type from |now| until |now| + |duration|.
  void ThrottleType(base::TimeDelta duration, base::TimeTicks now);

  // Backs off the type from |now| until |now| + |duration|.
  void BackOffType(base::TimeDelta duration, base::TimeTicks now);

  // Unblocks the type if base::TimeTicks::Now() >= |unblock_time_| expiry time.
  void UpdateThrottleOrBackoffState();

  // Update the local change nudge delay for this type.
  void UpdateLocalNudgeDelay(base::TimeDelta delay);

  // Return the BlockingMode for this type.
  WaitInterval::BlockingMode GetBlockingMode() const;

 private:
  friend class SyncSchedulerImplTest;

  // Number of local change nudges received for this type since the last
  // successful sync cycle.
  int local_nudge_count_;

  // Number of local refresh requests received for this type since the last
  // successful sync cycle.
  int local_refresh_request_count_;

  // The list of invalidations received since the last successful sync cycle.
  // This list may be incomplete.  See also:
  // drop_tracker_.IsRecoveringFromDropEvent() and server_payload_overflow_.
  //
  // This list takes ownership of its contents.
  std::vector<std::unique_ptr<InvalidationInterface>> pending_invalidations_;

  size_t payload_buffer_size_;

  // Set to true if this type is ready for, but has not yet completed initial
  // sync.
  bool initial_sync_required_;

  // Set to true if this type need to get update to resolve conflict issue.
  bool sync_required_to_resolve_conflict_;

  // If !unblock_time_.is_null(), this type is throttled or backed off, check
  // |wait_interval_->mode| for specific reason. Now the datatype may not
  // download or commit data until the specified time.
  base::TimeTicks unblock_time_;

  // Current wait state.  Null if we're not in backoff or throttling.
  std::unique_ptr<WaitInterval> wait_interval_;

  // A helper to keep track invalidations we dropped due to overflow.
  std::unique_ptr<InvalidationInterface> last_dropped_invalidation_;

  // The amount of time to delay a sync cycle by when a local change for this
  // type occurs.
  base::TimeDelta nudge_delay_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeTracker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_DATA_TYPE_TRACKER_H_
