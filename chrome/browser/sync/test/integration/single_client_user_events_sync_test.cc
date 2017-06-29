// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"

using fake_server::FakeServer;
using sync_pb::UserEventSpecifics;
using sync_pb::SyncEntity;

namespace {

class UserEventEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  UserEventEqualityChecker(browser_sync::ProfileSyncService* service,
                           FakeServer* fake_server,
                           std::vector<UserEventSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {
    for (const UserEventSpecifics& specifics : expected_specifics) {
      expected_specifics_[specifics.event_time_usec()] = specifics;
    }
  }

  bool IsExitConditionSatisfied() override {
    std::vector<SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::USER_EVENTS);

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_GE(expected_specifics_.size(), entities.size());

    if (expected_specifics_.size() > entities.size()) {
      return false;
    }

    for (const SyncEntity& entity : entities) {
      UserEventSpecifics server_specifics = entity.specifics().user_event();
      auto iter = expected_specifics_.find(server_specifics.event_time_usec());
      // We don't expect to encounter id matching events with different values,
      // this isn't going to recover so fail the test case now.
      CHECK(expected_specifics_.end() != iter);
      // TODO(skym): This may need to change if we start updating navigation_id
      // based on what sessions data is committed, and end up committing the
      // same event multiple times.
      EXPECT_EQ(iter->second.navigation_id(), server_specifics.navigation_id());
      EXPECT_EQ(iter->second.event_case(), server_specifics.event_case());
    }

    return true;
  }

  std::string GetDebugMessage() const override {
    return "Waiting server side USER_EVENTS to match expected.";
  }

 private:
  FakeServer* fake_server_;
  std::map<int64_t, UserEventSpecifics> expected_specifics_;
};

class SingleClientUserEventsSyncTest : public SyncTest {
 public:
  SingleClientUserEventsSyncTest() : SyncTest(SINGLE_CLIENT) {
    DisableVerifier();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientUserEventsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(
      0u,
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::USER_EVENTS).size());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  UserEventSpecifics specifics1;
  specifics1.set_event_time_usec(base::Time::Now().ToInternalValue());
  specifics1.mutable_test_event();
  event_service->RecordUserEvent(specifics1);
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics1})
      .Wait();
}

}  // namespace
