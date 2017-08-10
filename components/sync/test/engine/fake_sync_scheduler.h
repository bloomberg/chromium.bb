// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
#define COMPONENTS_SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_

#include <map>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "components/sync/engine_impl/sync_scheduler.h"

namespace syncer {

// A fake implementation of the SyncScheduler. If needed, we should add default
// logic needed for tests (invoking callbacks, etc) here rather than in higher
// level test classes.
class FakeSyncScheduler : public SyncScheduler {
 public:
  FakeSyncScheduler();
  ~FakeSyncScheduler() override;

  void Start(Mode mode, base::Time last_poll_time) override;
  void Stop() override;
  void ScheduleLocalNudge(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) override;
  void ScheduleLocalRefreshRequest(
      ModelTypeSet types,
      const tracked_objects::Location& nudge_location) override;
  void ScheduleInvalidationNudge(
      ModelType type,
      std::unique_ptr<InvalidationInterface> interface,
      const tracked_objects::Location& nudge_location) override;
  void ScheduleConfiguration(const ConfigurationParams& params) override;
  void ScheduleClearServerData(const ClearParams& params) override;

  void ScheduleInitialSyncNudge(ModelType model_type) override;
  void SetNotificationsEnabled(bool notifications_enabled) override;

  void OnCredentialsUpdated() override;
  void OnConnectionStatusChange() override;

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override;
  void OnTypesThrottled(ModelTypeSet types,
                        const base::TimeDelta& throttle_duration) override;
  void OnTypesBackedOff(ModelTypeSet types) override;
  bool IsAnyThrottleOrBackoff() override;
  void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  void OnReceivedCustomNudgeDelays(
      const std::map<ModelType, base::TimeDelta>& nudge_delays) override;
  void OnReceivedClientInvalidationHintBufferSize(int size) override;
  void OnSyncProtocolError(const SyncProtocolError& error) override;
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override;
  void OnReceivedMigrationRequest(ModelTypeSet types) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_FAKE_SYNC_SCHEDULER_H_
