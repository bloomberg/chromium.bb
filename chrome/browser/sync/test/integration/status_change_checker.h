// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_

#include <string>

class ProfileSyncServiceObserver;

// Interface for a helper class that can be used to check if a desired change in
// the state of the sync engine has taken place. Used by the desktop sync
// integration tests.
//
// Usage: Tests that want to use this class to wait for an arbitrary sync state
// must implement a concrete StatusChangeChecker object and pass it to
// ProfileSyncServiceHarness::AwaitStatusChange().
class StatusChangeChecker {
 public:
  explicit StatusChangeChecker();

  // Called every time ProfileSyncServiceHarness is notified of a change in the
  // state of the sync engine. Returns true if the desired change has occurred.
  virtual bool IsExitConditionSatisfied() = 0;

  // Returns a string representing this current StatusChangeChecker, and
  // possibly some small part of its state.  For example: "AwaitPassphraseError"
  // or "AwaitMigrationDone(BOOKMARKS)".
  virtual std::string GetDebugMessage() const = 0;

  virtual void InitObserver(ProfileSyncServiceObserver*) = 0;
  virtual void UninitObserver(ProfileSyncServiceObserver*) = 0;

 protected:
  virtual ~StatusChangeChecker();
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_STATUS_CHANGE_CHECKER_H_
