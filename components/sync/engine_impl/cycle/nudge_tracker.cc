// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/nudge_tracker.h"

#include <algorithm>
#include <utility>

#include "components/sync/engine/polling_constants.h"

namespace syncer {

namespace {

// Delays for syncer nudges.
const int kDefaultNudgeDelayMilliseconds = 200;
const int kSlowNudgeDelayMilliseconds = 2000;
const int kSyncRefreshDelayMilliseconds = 500;
const int kSyncSchedulerDelayMilliseconds = 250;

base::TimeDelta GetDefaultDelayForType(ModelType model_type,
                                       base::TimeDelta minimum_delay) {
  switch (model_type) {
    case AUTOFILL:
    case USER_EVENTS:
      // Accompany types rely on nudges from other types, and hence have long
      // nudge delays.
      return base::TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds);
    case BOOKMARKS:
    case PREFERENCES:
    case SESSIONS:
    case FAVICON_IMAGES:
    case FAVICON_TRACKING:
      // Types with sometimes automatic changes get longer delays to allow more
      // coalescing.
      return base::TimeDelta::FromMilliseconds(kSlowNudgeDelayMilliseconds);
    default:
      return minimum_delay;
  }
}

}  // namespace

NudgeTracker::NudgeTracker()
    : invalidations_enabled_(false),
      invalidations_out_of_sync_(true),
      minimum_local_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kDefaultNudgeDelayMilliseconds)),
      local_refresh_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kSyncRefreshDelayMilliseconds)),
      remote_invalidation_nudge_delay_(
          base::TimeDelta::FromMilliseconds(kSyncSchedulerDelayMilliseconds)) {
  ModelTypeSet protocol_types = ProtocolTypes();
  // Default initialize all the type trackers.
  for (ModelTypeSet::Iterator it = protocol_types.First(); it.Good();
       it.Inc()) {
    type_trackers_.insert(
        std::make_pair(it.Get(), std::make_unique<DataTypeTracker>()));
  }
}

NudgeTracker::~NudgeTracker() {}

bool NudgeTracker::IsSyncRequired() const {
  if (IsRetryRequired())
    return true;

  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->IsSyncRequired()) {
      return true;
    }
  }

  return false;
}

bool NudgeTracker::IsGetUpdatesRequired() const {
  if (invalidations_out_of_sync_)
    return true;

  if (IsRetryRequired())
    return true;

  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->IsGetUpdatesRequired()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsRetryRequired() const {
  if (sync_cycle_start_time_.is_null())
    return false;

  if (current_retry_time_.is_null())
    return false;

  return current_retry_time_ <= sync_cycle_start_time_;
}

void NudgeTracker::RecordSuccessfulSyncCycle() {
  // If a retry was required, we've just serviced it.  Unset the flag.
  if (IsRetryRequired())
    current_retry_time_ = base::TimeTicks();

  // A successful cycle while invalidations are enabled puts us back into sync.
  invalidations_out_of_sync_ = !invalidations_enabled_;

  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second->RecordSuccessfulSyncCycle();
  }
}

base::TimeDelta NudgeTracker::RecordLocalChange(ModelTypeSet types) {
  // Start with the longest delay.
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds);
  for (ModelTypeSet::Iterator type_it = types.First(); type_it.Good();
       type_it.Inc()) {
    TypeTrackerMap::const_iterator tracker_it =
        type_trackers_.find(type_it.Get());
    DCHECK(tracker_it != type_trackers_.end());

    // Only if the type tracker has a valid delay (non-zero) that is shorter
    // than the calculated delay do we update the calculated delay.
    base::TimeDelta type_delay = tracker_it->second->RecordLocalChange();
    if (type_delay.is_zero()) {
      type_delay =
          GetDefaultDelayForType(type_it.Get(), minimum_local_nudge_delay_);
    }
    if (type_delay < delay)
      delay = type_delay;
  }
  return delay;
}

base::TimeDelta NudgeTracker::RecordLocalRefreshRequest(ModelTypeSet types) {
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(it.Get());
    DCHECK(tracker_it != type_trackers_.end()) << ModelTypeToString(it.Get());
    tracker_it->second->RecordLocalRefreshRequest();
  }
  return local_refresh_nudge_delay_;
}

base::TimeDelta NudgeTracker::RecordRemoteInvalidation(
    ModelType type,
    std::unique_ptr<InvalidationInterface> invalidation) {
  // Forward the invalidations to the proper recipient.
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordRemoteInvalidation(std::move(invalidation));
  return remote_invalidation_nudge_delay_;
}

void NudgeTracker::RecordInitialSyncRequired(ModelType type) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordInitialSyncRequired();
}

void NudgeTracker::RecordCommitConflict(ModelType type) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->RecordCommitConflict();
}

void NudgeTracker::OnInvalidationsEnabled() {
  invalidations_enabled_ = true;
}

void NudgeTracker::OnInvalidationsDisabled() {
  invalidations_enabled_ = false;
  invalidations_out_of_sync_ = true;
}

void NudgeTracker::SetTypesThrottledUntil(ModelTypeSet types,
                                          base::TimeDelta length,
                                          base::TimeTicks now) {
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(it.Get());
    tracker_it->second->ThrottleType(length, now);
  }
}

void NudgeTracker::SetTypeBackedOff(ModelType type,
                                    base::TimeDelta length,
                                    base::TimeTicks now) {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());
  tracker_it->second->BackOffType(length, now);
}

void NudgeTracker::UpdateTypeThrottlingAndBackoffState() {
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second->UpdateThrottleOrBackoffState();
  }
}

bool NudgeTracker::IsAnyTypeBlocked() const {
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->IsBlocked()) {
      return true;
    }
  }
  return false;
}

bool NudgeTracker::IsTypeBlocked(ModelType type) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  return type_trackers_.find(type)->second->IsBlocked();
}

WaitInterval::BlockingMode NudgeTracker::GetTypeBlockingMode(
    ModelType type) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  return type_trackers_.find(type)->second->GetBlockingMode();
}

base::TimeDelta NudgeTracker::GetTimeUntilNextUnblock() const {
  DCHECK(IsAnyTypeBlocked()) << "This function requires a pending unblock.";

  // Return min of GetTimeUntilUnblock() values for all IsBlocked() types.
  base::TimeDelta time_until_next_unblock = base::TimeDelta::Max();
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->IsBlocked()) {
      time_until_next_unblock =
          std::min(time_until_next_unblock, it->second->GetTimeUntilUnblock());
    }
  }
  DCHECK(!time_until_next_unblock.is_max());

  return time_until_next_unblock;
}

base::TimeDelta NudgeTracker::GetTypeLastBackoffInterval(ModelType type) const {
  TypeTrackerMap::const_iterator tracker_it = type_trackers_.find(type);
  DCHECK(tracker_it != type_trackers_.end());

  return tracker_it->second->GetLastBackoffInterval();
}

ModelTypeSet NudgeTracker::GetBlockedTypes() const {
  ModelTypeSet result;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->IsBlocked()) {
      result.Put(it->first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetNudgedTypes() const {
  ModelTypeSet result;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->HasLocalChangePending()) {
      result.Put(it->first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetNotifiedTypes() const {
  ModelTypeSet result;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->HasPendingInvalidation()) {
      result.Put(it->first);
    }
  }
  return result;
}

ModelTypeSet NudgeTracker::GetRefreshRequestedTypes() const {
  ModelTypeSet result;
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    if (it->second->HasRefreshRequestPending()) {
      result.Put(it->first);
    }
  }
  return result;
}

void NudgeTracker::SetLegacyNotificationHint(
    ModelType type,
    sync_pb::DataTypeProgressMarker* progress) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());
  type_trackers_.find(type)->second->SetLegacyNotificationHint(progress);
}

sync_pb::GetUpdatesCallerInfo::GetUpdatesSource NudgeTracker::GetLegacySource()
    const {
  // There's an order to these sources: NOTIFICATION, DATATYPE_REFRESH, LOCAL,
  // RETRY.  The server makes optimization decisions based on this field, so
  // it's important to get this right.  Setting it wrong could lead to missed
  // updates.
  //
  // This complexity is part of the reason why we're deprecating 'source' in
  // favor of 'origin'.
  bool has_invalidation_pending = false;
  bool has_refresh_request_pending = false;
  bool has_commit_pending = false;
  bool is_initial_sync_required = false;
  bool has_retry = IsRetryRequired();

  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    const DataTypeTracker& tracker = *it->second;
    if (!tracker.IsBlocked() && tracker.HasPendingInvalidation()) {
      has_invalidation_pending = true;
    }
    if (!tracker.IsBlocked() && tracker.HasRefreshRequestPending()) {
      has_refresh_request_pending = true;
    }
    if (!tracker.IsBlocked() && tracker.HasLocalChangePending()) {
      has_commit_pending = true;
    }
    if (!tracker.IsBlocked() && tracker.IsInitialSyncRequired()) {
      is_initial_sync_required = true;
    }
  }

  if (has_invalidation_pending) {
    return sync_pb::GetUpdatesCallerInfo::NOTIFICATION;
  } else if (has_refresh_request_pending) {
    return sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH;
  } else if (is_initial_sync_required) {
    // Not quite accurate, but good enough for our purposes.  This setting of
    // SOURCE is just a backward-compatibility hack anyway.
    return sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH;
  } else if (has_commit_pending) {
    return sync_pb::GetUpdatesCallerInfo::LOCAL;
  } else if (has_retry) {
    return sync_pb::GetUpdatesCallerInfo::RETRY;
  } else {
    return sync_pb::GetUpdatesCallerInfo::UNKNOWN;
  }
}

void NudgeTracker::FillProtoMessage(ModelType type,
                                    sync_pb::GetUpdateTriggers* msg) const {
  DCHECK(type_trackers_.find(type) != type_trackers_.end());

  // Fill what we can from the global data.
  msg->set_invalidations_out_of_sync(invalidations_out_of_sync_);

  // Delegate the type-specific work to the DataTypeTracker class.
  type_trackers_.find(type)->second->FillGetUpdatesTriggersMessage(msg);
}

void NudgeTracker::SetSyncCycleStartTime(base::TimeTicks now) {
  sync_cycle_start_time_ = now;

  // If current_retry_time_ is still set, that means we have an old retry time
  // left over from a previous cycle.  For example, maybe we tried to perform
  // this retry, hit a network connection error, and now we're in exponential
  // backoff.  In that case, we want this sync cycle to include the GU retry
  // flag so we leave this variable set regardless of whether or not there is an
  // overwrite pending.
  if (!current_retry_time_.is_null()) {
    return;
  }

  // If do not have a current_retry_time_, but we do have a next_retry_time_ and
  // it is ready to go, then we set it as the current_retry_time_.  It will stay
  // there until a GU retry has succeeded.
  if (!next_retry_time_.is_null() &&
      next_retry_time_ <= sync_cycle_start_time_) {
    current_retry_time_ = next_retry_time_;
    next_retry_time_ = base::TimeTicks();
  }
}

void NudgeTracker::SetHintBufferSize(size_t size) {
  for (TypeTrackerMap::const_iterator it = type_trackers_.begin();
       it != type_trackers_.end(); ++it) {
    it->second->UpdatePayloadBufferSize(size);
  }
}

void NudgeTracker::SetNextRetryTime(base::TimeTicks retry_time) {
  next_retry_time_ = retry_time;
}

void NudgeTracker::OnReceivedCustomNudgeDelays(
    const std::map<ModelType, base::TimeDelta>& delay_map) {
  for (std::map<ModelType, base::TimeDelta>::const_iterator iter =
           delay_map.begin();
       iter != delay_map.end(); ++iter) {
    ModelType type = iter->first;
    DCHECK(ProtocolTypes().Has(type));
    TypeTrackerMap::const_iterator type_iter = type_trackers_.find(type);
    if (type_iter == type_trackers_.end())
      continue;

    if (iter->second > minimum_local_nudge_delay_) {
      type_iter->second->UpdateLocalNudgeDelay(iter->second);
    } else {
      type_iter->second->UpdateLocalNudgeDelay(
          GetDefaultDelayForType(type, minimum_local_nudge_delay_));
    }
  }
}

void NudgeTracker::SetDefaultNudgeDelay(base::TimeDelta nudge_delay) {
  minimum_local_nudge_delay_ = nudge_delay;
}

}  // namespace syncer
