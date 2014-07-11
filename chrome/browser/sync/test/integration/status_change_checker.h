// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_

#include <string>

#include "base/time/time.h"

class ProfileSyncServiceHarness;

// Interface for a helper class that can pump the message loop while waiting
// for a certain state transition to take place.
//
// This is a template that should be filled in by child classes so they can
// observe specific kinds of changes and await specific conditions.
//
// The instances of this class are intended to be single-use.  It doesn't make
// sense to call StartBlockingWait() more than once.
class StatusChangeChecker {
 public:
  explicit StatusChangeChecker();

  // Returns a string representing this current StatusChangeChecker, and
  // possibly some small part of its state.  For example: "AwaitPassphraseError"
  // or "AwaitMigrationDone(BOOKMARKS)".
  virtual std::string GetDebugMessage() const = 0;

  // Returns true if the blocking wait was exited because of a timeout.
  bool TimedOut() const;

  virtual bool IsExitConditionSatisfied() = 0;

 protected:
  virtual ~StatusChangeChecker();

  // Timeout length when blocking.
  virtual base::TimeDelta GetTimeoutDuration();

  // Helper function to start running the nested message loop.
  //
  // Will exit if IsExitConditionSatisfied() returns true when called from
  // CheckExitCondition(), if a timeout occurs, or if StopWaiting() is called.
  //
  // The timeout length is specified with GetTimeoutDuration().
  void StartBlockingWait();

  // Stop the nested running of the message loop started in StartBlockingWait().
  void StopWaiting();

  // Checks IsExitConditionSatisfied() and calls StopWaiting() if it returns
  // true.
  void CheckExitCondition();

  // Called when the blocking wait timeout is exceeded.
  void OnTimeout();

  bool timed_out_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
