// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.cc"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"

using base::Time;
using base::TimeDelta;
using fake_server::FakeServer;
using sync_pb::UserEventSpecifics;
using sync_pb::SyncEntity;
using sync_pb::CommitResponse;

namespace {

UserEventSpecifics CreateEvent(int minutes_ago) {
  UserEventSpecifics specifics;
  specifics.set_event_time_usec(
      (Time::Now() - TimeDelta::FromMinutes(minutes_ago)).ToInternalValue());
  specifics.mutable_test_event();
  return specifics;
}

CommitResponse::ResponseType BounceType(
    CommitResponse::ResponseType type,
    const syncer::LoopbackServerEntity& entity) {
  return type;
}

CommitResponse::ResponseType TransientErrorFirst(
    bool* first,
    UserEventSpecifics* retry_specifics,
    const syncer::LoopbackServerEntity& entity) {
  if (*first) {
    *first = false;
    SyncEntity sync_entity;
    entity.SerializeAsProto(&sync_entity);
    *retry_specifics = sync_entity.specifics().user_event();
    return CommitResponse::TRANSIENT_ERROR;
  } else {
    return CommitResponse::SUCCESS;
  }
}

class UserEventEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  UserEventEqualityChecker(browser_sync::ProfileSyncService* service,
                           FakeServer* fake_server,
                           std::vector<UserEventSpecifics> expected_specifics)
      : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {
    for (const UserEventSpecifics& specifics : expected_specifics) {
      expected_specifics_.insert(std::pair<int64_t, UserEventSpecifics>(
          specifics.event_time_usec(), specifics));
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

    // Because we have a multimap, we cannot just check counts and equality of
    // items, but we need to make sure 2 of A and 1 of B is not the same as 1 of
    // A and 2 of B. So to make this easy, copy the multimap and remove items.
    std::multimap<int64_t, UserEventSpecifics> copied_expected_(
        expected_specifics_.begin(), expected_specifics_.end());
    for (const SyncEntity& entity : entities) {
      UserEventSpecifics server_specifics = entity.specifics().user_event();
      auto iter = copied_expected_.find(server_specifics.event_time_usec());
      // We don't expect to encounter id matching events with different values,
      // this isn't going to recover so fail the test case now.
      EXPECT_TRUE(copied_expected_.end() != iter);
      // TODO(skym): This may need to change if we start updating navigation_id
      // based on what sessions data is committed, and end up committing the
      // same event multiple times.
      EXPECT_EQ(iter->second.navigation_id(), server_specifics.navigation_id());
      EXPECT_EQ(iter->second.event_case(), server_specifics.event_case());

      copied_expected_.erase(iter);
    }

    return true;
  }

  std::string GetDebugMessage() const override {
    return "Waiting server side USER_EVENTS to match expected.";
  }

 private:
  FakeServer* fake_server_;
  std::multimap<int64_t, UserEventSpecifics> expected_specifics_;
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
  const UserEventSpecifics specifics = CreateEvent(0);
  event_service->RecordUserEvent(specifics);
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics})
      .Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, RetrySequential) {
  ASSERT_TRUE(SetupSync());
  const UserEventSpecifics specifics1 = CreateEvent(1);
  const UserEventSpecifics specifics2 = CreateEvent(2);
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  GetFakeServer()->OverrideResponseType(
      base::Bind(&BounceType, CommitResponse::TRANSIENT_ERROR));
  event_service->RecordUserEvent(specifics1);

  // This will block until we hit a TRANSIENT_ERROR, at which point we will
  // regain control and can switch back to SUCCESS.
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics1})
      .Wait();
  GetFakeServer()->OverrideResponseType(
      base::Bind(&BounceType, CommitResponse::SUCCESS));
  // Because the fake server records commits even on failure, we are able to
  // verify that the commit for this event reached the server twice.
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                           {specifics1, specifics1})
      .Wait();

  // Only record |specifics2| after |specifics1| was successful to avoid race
  // conditions.
  event_service->RecordUserEvent(specifics2);
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                           {specifics1, specifics1, specifics2})
      .Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, RetryParallel) {
  ASSERT_TRUE(SetupSync());
  bool first = true;
  const UserEventSpecifics specifics1 = CreateEvent(1);
  const UserEventSpecifics specifics2 = CreateEvent(2);
  UserEventSpecifics retry_specifics;

  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  // We're not really sure if |specifics1| or |specifics2| is going to see the
  // error, so record the one that does into |retry_specifics| and use it in
  // expectations.
  GetFakeServer()->OverrideResponseType(
      base::Bind(&TransientErrorFirst, &first, &retry_specifics));

  event_service->RecordUserEvent(specifics2);
  event_service->RecordUserEvent(specifics1);
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                           {specifics1, specifics2})
      .Wait();
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                           {specifics1, specifics2, retry_specifics})
      .Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoHistory) {
  const UserEventSpecifics specifics1 = CreateEvent(1);
  const UserEventSpecifics specifics2 = CreateEvent(2);
  const UserEventSpecifics specifics3 = CreateEvent(3);
  ASSERT_TRUE(SetupSync());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  event_service->RecordUserEvent(specifics1);
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::TYPED_URLS));
  event_service->RecordUserEvent(specifics2);
  ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(syncer::TYPED_URLS));
  event_service->RecordUserEvent(specifics3);

  // No |specifics2| because it was recorded while history was disabled.
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(),
                           {specifics1, specifics3})
      .Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, NoSessions) {
  const UserEventSpecifics specifics = CreateEvent(1);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::PROXY_TABS));
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));

  event_service->RecordUserEvent(specifics);

  // PROXY_TABS shouldn't affect us in any way.
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics})
      .Wait();
}

IN_PROC_BROWSER_TEST_F(SingleClientUserEventsSyncTest, Encryption) {
  const UserEventSpecifics specifics1 = CreateEvent(1);
  const UserEventSpecifics specifics2 = CreateEvent(2);
  const GURL url("http://www.one.com/");

  ASSERT_TRUE(SetupSync());
  syncer::UserEventService* event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  event_service->RecordUserEvent(specifics1);
  ASSERT_TRUE(EnableEncryption(0));
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics1})
      .Wait();
  event_service->RecordUserEvent(specifics2);

  // Just checking that we don't see specifics2 isn't very convincing yet,
  // because it may simply not have reached the server yet. So lets send
  // something else through the system that we can wait on before checking.
  // Tab/SESSIONS data was picked fairly arbitrarily, note that we expect 2
  // entries, one for the window/header and one for the tab.
  sessions_helper::OpenTab(0, url);
  ServerCountMatchStatusChecker(syncer::SESSIONS, 2);
  UserEventEqualityChecker(GetSyncService(0), GetFakeServer(), {specifics1})
      .Wait();
}

}  // namespace
