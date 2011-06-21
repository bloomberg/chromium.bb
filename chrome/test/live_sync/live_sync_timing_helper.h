// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TIMING_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TIMING_HELPER_H_
#pragma once

#include "base/basictypes.h"

#include <vector>

namespace base {
class TimeDelta;
}

class ProfileSyncServiceHarness;

class LiveSyncTimingHelper {
 public:
  LiveSyncTimingHelper();
  ~LiveSyncTimingHelper();

  // Returns the time taken for |client| to complete a single sync cycle.
  static base::TimeDelta TimeSyncCycle(ProfileSyncServiceHarness* client);

  // Returns the time taken for both |client| and |partner| to complete a sync
  // cycle.
  static base::TimeDelta TimeMutualSyncCycle(
      ProfileSyncServiceHarness* client, ProfileSyncServiceHarness* partner);

  // Returns the time taken for all clients in |clients| to complete their
  // respective sync cycles.
  static base::TimeDelta TimeUntilQuiescence(
      std::vector<ProfileSyncServiceHarness*>& clients);

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTimingHelper);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TIMING_HELPER_H_
