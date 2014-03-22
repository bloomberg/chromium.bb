// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"

class ProfileSyncService;
class ProfileSyncServiceHarness;

// This class provides some common functionality for StatusChangeCheckers that
// observe only one ProfileSyncService.  This class is abstract.  Its
// descendants are expected to provide additional functionality.
class SingleClientStatusChangeChecker : public StatusChangeChecker {
 public:
  explicit SingleClientStatusChangeChecker(ProfileSyncService* service);
  virtual ~SingleClientStatusChangeChecker();

  // StatusChangeChecker implementations and stubs.
  virtual bool IsExitConditionSatisfied() = 0;
  virtual std::string GetDebugMessage() const = 0;
  virtual void InitObserver(ProfileSyncServiceHarness* obs) OVERRIDE;
  virtual void UninitObserver(ProfileSyncServiceHarness* obs) OVERRIDE;

 protected:
  ProfileSyncService* service() { return service_; }

 private:
  ProfileSyncService* service_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
