// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_

#include "base/basictypes.h"

#include <string>
#include <vector>

namespace base {
class TimeDelta;
}

class ProfileSyncServiceHarness;

class SyncTimingHelper {
 public:
  SyncTimingHelper();
  ~SyncTimingHelper();

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

  // Print a timing measurement in a format appropriate for the chromium perf
  // dashboard.  Simplified version of methods defined in
  // chrome/test/ui/ui_perf_test.{h,cc}.
  static void PrintResult(const std::string& measurement,
                          const std::string& trace,
                          const base::TimeDelta& dt);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncTimingHelper);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_
