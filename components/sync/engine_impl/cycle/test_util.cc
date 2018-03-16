// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/test_util.h"

namespace syncer {
namespace test_util {

void SimulateGetEncryptionKeyFailed(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SERVER_RESPONSE_VALIDATION_FAILED);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateConfigureSuccess(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateConfigureFailed(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateConfigureConnectionFailure(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      NETWORK_CONNECTION_UNAVAILABLE);
}

void SimulateNormalSuccess(ModelTypeSet requested_types,
                           NudgeTracker* nudge_tracker,
                           SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_commit_result(SYNCER_OK);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateDownloadUpdatesFailed(ModelTypeSet requested_types,
                                   NudgeTracker* nudge_tracker,
                                   SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateCommitFailed(ModelTypeSet requested_types,
                          NudgeTracker* nudge_tracker,
                          SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
  cycle->mutable_status_controller()->set_commit_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateConnectionFailure(ModelTypeSet requested_types,
                               NudgeTracker* nudge_tracker,
                               SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      NETWORK_CONNECTION_UNAVAILABLE);
}

void SimulatePollSuccess(ModelTypeSet requested_types, SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulatePollFailed(ModelTypeSet requested_types, SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateThrottledImpl(SyncCycle* cycle, const base::TimeDelta& delta) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_THROTTLED);
  cycle->delegate()->OnThrottled(delta);
}

void SimulateTypesThrottledImpl(SyncCycle* cycle,
                                ModelTypeSet types,
                                const base::TimeDelta& delta) {
  cycle->mutable_status_controller()->set_commit_result(SYNCER_OK);
  cycle->delegate()->OnTypesThrottled(types, delta);
}

void SimulatePartialFailureImpl(SyncCycle* cycle, ModelTypeSet types) {
  cycle->mutable_status_controller()->set_commit_result(SYNCER_OK);
  cycle->delegate()->OnTypesBackedOff(types);
}

void SimulatePollIntervalUpdateImpl(ModelTypeSet requested_types,
                                    SyncCycle* cycle,
                                    const base::TimeDelta& new_poll) {
  SimulatePollSuccess(requested_types, cycle);
  cycle->delegate()->OnReceivedLongPollIntervalUpdate(new_poll);
}

void SimulateGuRetryDelayCommandImpl(SyncCycle* cycle, base::TimeDelta delay) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
  cycle->delegate()->OnReceivedGuRetryDelay(delay);
}

}  // namespace test_util
}  // namespace syncer
