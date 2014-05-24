// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"

class ProfileSyncService;

// This class provides some common functionality for StatusChangeCheckers that
// observe many ProfileSyncServices.  This class is abstract.  Its descendants
// are expected to provide additional functionality.
class MultiClientStatusChangeChecker
  : public StatusChangeChecker,
    public ProfileSyncServiceObserver {
 public:
  explicit MultiClientStatusChangeChecker(
      std::vector<ProfileSyncService*> services);
  virtual ~MultiClientStatusChangeChecker();

  // Called when waiting times out.
  void OnTimeout();

  // Blocks until the exit condition is satisfied or a timeout occurs.
  void Wait();

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // StatusChangeChecker implementations and stubs.
  virtual bool IsExitConditionSatisfied() = 0;
  virtual std::string GetDebugMessage() const = 0;

 protected:
  const std::vector<ProfileSyncService*>& services() { return services_; }

 private:
  std::vector<ProfileSyncService*> services_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MULTI_CLIENT_STATUS_CHANGE_CHECKER_H_
