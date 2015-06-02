// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_MOCK_SYNC_SCHEDULER_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_MOCK_SYNC_SCHEDULER_H

#include "components/proximity_auth/cryptauth/sync_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace proximity_auth {

// Mock implementation of SyncScheduler.
class MockSyncScheduler : public SyncScheduler {
 public:
  MockSyncScheduler();
  ~MockSyncScheduler() override;

  // SyncScheduler:
  MOCK_METHOD2(Start,
               void(const base::TimeDelta& elapsed_time_since_last_sync,
                    Strategy strategy));
  MOCK_METHOD0(ForceSync, void(void));
  MOCK_CONST_METHOD0(GetTimeToNextSync, base::TimeDelta(void));
  MOCK_CONST_METHOD0(GetStrategy, Strategy(void));
  MOCK_CONST_METHOD0(GetSyncState, SyncState(void));
  MOCK_METHOD1(OnSyncCompleted, void(bool success));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncScheduler);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_CRYPTAUTH_MOCK_SYNC_SCHEDULER_H
