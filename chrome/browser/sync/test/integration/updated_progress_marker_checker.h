// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

// Waits until the latest progress markers are available.
//
// There are several limitations to this checker:
// - It assumes that this client is the only one committing at this time.
// - It relies on the test-only 'self-notify' to trigger an extra GetUpdate
//   cycle after every commit.
// - It's flaky.  In some rare cases, the IsExitConditionSatisifed() call could
//   return a false positive.  See comments in the .cc file for details.
//
// Because of these limitations, we intend to eventually migrate all tests off
// of this checker.  Please do not use it in new tests.
class UpdatedProgressMarkerChecker : public SingleClientStatusChangeChecker {
 public:
  explicit UpdatedProgressMarkerChecker(ProfileSyncService* service);
  virtual ~UpdatedProgressMarkerChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_UPDATED_PROGRESS_MARKER_CHECKER_H_
