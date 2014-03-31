// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"

namespace sync_integration_test_util {

class PassphraseRequiredChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseRequiredChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    return service()->IsPassphraseRequired();
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "Passhrase Required";
  }
};

class PassphraseAcceptedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseAcceptedChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    return !service()->IsPassphraseRequired() &&
        service()->IsUsingSecondaryPassphrase();
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "Passhrase Accepted";
  }
};

bool AwaitPassphraseRequired(ProfileSyncService* service) {
  PassphraseRequiredChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitPassphraseAccepted(ProfileSyncService* service) {
  PassphraseAcceptedChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitCommitActivityCompletion(ProfileSyncService* service) {
  UpdatedProgressMarkerChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace sync_integration_test_util
