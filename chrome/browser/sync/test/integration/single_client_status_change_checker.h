// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"

class ProfileSyncService;
class ProfileSyncServiceHarness;

// This class provides some common functionality for StatusChangeCheckers that
// observe only one ProfileSyncService.  This class is abstract.  Its
// descendants are expected to provide additional functionality.
class SingleClientStatusChangeChecker
  : public StatusChangeChecker,
    public ProfileSyncServiceObserver {
 public:
  explicit SingleClientStatusChangeChecker(ProfileSyncService* service);
  virtual ~SingleClientStatusChangeChecker();

  // Timeout length for this operation.  Default is 45s.
  virtual base::TimeDelta GetTimeoutDuration();

  // Called when waiting times out.
  void OnTimeout();

  // Blocks until the exit condition is satisfied or a timeout occurs.
  void Await();

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // Returns true if the checker timed out.
  bool TimedOut();

  // StatusChangeChecker implementations and stubs.
  virtual bool IsExitConditionSatisfied() = 0;
  virtual std::string GetDebugMessage() const = 0;

 protected:
  ProfileSyncService* service() { return service_; }

 private:
  ProfileSyncService* service_;
  bool timed_out_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
