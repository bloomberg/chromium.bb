// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/browser/profile_sync_service.h"

namespace {

// Helper class to block until the server has a given number of entities.
class ServerCountMatchStatusChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  ServerCountMatchStatusChecker(syncer::ModelType type, size_t count)
      : type_(type), count_(count) {}
  ~ServerCountMatchStatusChecker() override {}

  bool IsExitConditionSatisfied() override {
    return count_ == fake_server()->GetSyncEntitiesByModelType(type_).size();
  }

  std::string GetDebugMessage() const override {
    return base::StringPrintf(
        "Waiting for fake server entity count %zu to match expected count %zu "
        "for type %d",
        (size_t)fake_server()->GetSyncEntitiesByModelType(type_).size(), count_,
        type_);
  }

 private:
  const syncer::ModelType type_;
  const size_t count_;
};

class PassphraseRequiredChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseRequiredChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return service()->IsPassphraseRequired();
  }

  std::string GetDebugMessage() const override { return "Passhrase Required"; }
};

class PassphraseAcceptedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseAcceptedChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return !service()->IsPassphraseRequired() &&
        service()->IsUsingSecondaryPassphrase();
  }

  std::string GetDebugMessage() const override { return "Passhrase Accepted"; }
};

}  // namespace

namespace sync_integration_test_util {

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

bool AwaitServerCount(syncer::ModelType type, size_t count) {
  ServerCountMatchStatusChecker checker(type, count);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace sync_integration_test_util
