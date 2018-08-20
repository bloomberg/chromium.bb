// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/fcm_sync_invalidation_listener.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/impl/unacked_invalidation_set_test_util.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using invalidation::ObjectId;

const char kPayload1[] = "payload1";
const char kPayload2[] = "payload2";

const int64_t kVersion1 = 1LL;
const int64_t kVersion2 = 2LL;

const int kChromeSyncSourceId = 1004;


// Fake invalidation::InvalidationClient implementation that keeps
// track of registered IDs and acked handles.
class FakeInvalidationClient : public InvalidationClient {
 public:
  FakeInvalidationClient() : started_(false) {}
  ~FakeInvalidationClient() override {}

  // invalidation::InvalidationClient implementation.

  void Start() override { started_ = true; }

  void Stop() override { started_ = false; }

 private:
  bool started_;
};

// Fake delegate that keeps track of invalidation counts, payloads,
// and state.
class FakeDelegate : public FCMSyncInvalidationListener::Delegate {
 public:
  explicit FakeDelegate(FCMSyncInvalidationListener* listener)
      : state_(TRANSIENT_INVALIDATION_ERROR) {}
  ~FakeDelegate() override {}

  size_t GetInvalidationCount(const ObjectId& id) const {
    Map::const_iterator it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      return 0;
    } else {
      return it->second.size();
    }
  }

  int64_t GetVersion(const ObjectId& id) const {
    Map::const_iterator it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return 0;
    } else {
      return it->second.back().version();
    }
  }

  std::string GetPayload(const ObjectId& id) const {
    Map::const_iterator it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return nullptr;
    } else {
      return it->second.back().payload();
    }
  }

  bool IsUnknownVersion(const ObjectId& id) const {
    Map::const_iterator it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return false;
    } else {
      return it->second.back().is_unknown_version();
    }
  }

  bool StartsWithUnknownVersion(const ObjectId& id) const {
    Map::const_iterator it = invalidations_.find(id);
    if (it == invalidations_.end()) {
      ADD_FAILURE() << "No invalidations for ID " << ObjectIdToString(id);
      return false;
    } else {
      return it->second.front().is_unknown_version();
    }
  }

  InvalidatorState GetInvalidatorState() const { return state_; }

  void AcknowledgeNthInvalidation(const ObjectId& id, size_t n) {
    List& list = invalidations_[id];
    List::iterator it = list.begin() + n;
    it->Acknowledge();
  }

  void AcknowledgeAll(const ObjectId& id) {
    List& list = invalidations_[id];
    for (List::iterator it = list.begin(); it != list.end(); ++it) {
      it->Acknowledge();
    }
  }

  void DropNthInvalidation(const ObjectId& id, size_t n) {
    List& list = invalidations_[id];
    List::iterator it = list.begin() + n;
    it->Drop();
    dropped_invalidations_map_.erase(id);
    dropped_invalidations_map_.insert(std::make_pair(id, *it));
  }

  void RecoverFromDropEvent(const ObjectId& id) {
    DropMap::iterator it = dropped_invalidations_map_.find(id);
    if (it != dropped_invalidations_map_.end()) {
      it->second.Acknowledge();
      dropped_invalidations_map_.erase(it);
    }
  }

  // FCMSyncInvalidationListener::Delegate implementation.
  void OnInvalidate(const ObjectIdInvalidationMap& invalidation_map) override {
    ObjectIdSet ids = invalidation_map.GetObjectIds();
    for (ObjectIdSet::iterator it = ids.begin(); it != ids.end(); ++it) {
      const SingleObjectInvalidationSet& incoming =
          invalidation_map.ForObject(*it);
      List& list = invalidations_[*it];
      list.insert(list.end(), incoming.begin(), incoming.end());
    }
  }

  void OnInvalidatorStateChange(InvalidatorState state) override {
    state_ = state;
  }

 private:
  typedef std::vector<Invalidation> List;
  typedef std::map<ObjectId, List, ObjectIdLessThan> Map;
  typedef std::map<ObjectId, Invalidation, ObjectIdLessThan> DropMap;

  Map invalidations_;
  InvalidatorState state_;
  DropMap dropped_invalidations_map_;
};

std::unique_ptr<InvalidationClient> CreateFakeInvalidationClient(
    FakeInvalidationClient** fake_invalidation_client,
    NetworkChannel* network_channel,
    Logger* logger,
    InvalidationListener* listener) {
  std::unique_ptr<FakeInvalidationClient> fake_client =
      std::make_unique<FakeInvalidationClient>();
  *fake_invalidation_client = fake_client.get();
  return fake_client;
}

class MockRegistrationManager : public PerUserTopicRegistrationManager {
 public:
  MockRegistrationManager()
      : PerUserTopicRegistrationManager(
            nullptr /* identity_provider */,
            nullptr /* pref_service */,
            nullptr /* loader_factory */,
            base::BindRepeating(&syncer::JsonUnsafeParser::Parse)) {}
  ~MockRegistrationManager() override {}
  MOCK_METHOD2(UpdateRegisteredIds,
               void(const InvalidationObjectIdSet& ids,
                    const std::string& token));
};

class FCMSyncInvalidationListenerTest : public testing::Test {
 protected:
  FCMSyncInvalidationListenerTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kExtensionsId_(kChromeSyncSourceId, "EXTENSION"),
        kAppsId_(kChromeSyncSourceId, "APP"),
        fcm_sync_network_channel_(new FCMSyncNetworkChannel()),
        fake_invalidation_client_(nullptr),
        listener_(base::WrapUnique(fcm_sync_network_channel_)),
        fake_delegate_(&listener_) {}

  void SetUp() override {
    StartClient();

    registered_ids_.insert(kBookmarksId_);
    registered_ids_.insert(kPreferencesId_);
    listener_.UpdateRegisteredIds(registered_ids_);
  }

  void TearDown() override { StopClient(); }

  // Restart client without re-registering IDs.
  void RestartClient() {
    StopClient();
    StartClient();
  }

  void StartClient() {
    fake_invalidation_client_ = nullptr;
    std::unique_ptr<MockRegistrationManager> mock_registration_manager =
        std::make_unique<MockRegistrationManager>();
    listener_.Start(base::BindOnce(&CreateFakeInvalidationClient,
                                   &fake_invalidation_client_),
                    &fake_delegate_, std::move(mock_registration_manager));
    DCHECK(fake_invalidation_client_);
  }

  void StopClient() {
    // listener_.StopForTest() stops the invalidation scheduler, which
    // deletes any pending tasks without running them.  Some tasks
    // "run and delete" another task, so they must be run in order to
    // avoid leaking the inner task.  listener_.StopForTest() does not
    // schedule any tasks, so it's both necessary and sufficient to
    // drain the task queue before calling it.
    fake_invalidation_client_ = nullptr;
    listener_.StopForTest();
  }

  size_t GetInvalidationCount(const ObjectId& id) const {
    return fake_delegate_.GetInvalidationCount(id);
  }

  int64_t GetVersion(const ObjectId& id) const {
    return fake_delegate_.GetVersion(id);
  }

  std::string GetPayload(const ObjectId& id) const {
    return fake_delegate_.GetPayload(id);
  }

  bool IsUnknownVersion(const ObjectId& id) const {
    return fake_delegate_.IsUnknownVersion(id);
  }

  bool StartsWithUnknownVersion(const ObjectId& id) const {
    return fake_delegate_.StartsWithUnknownVersion(id);
  }

  void AcknowledgeNthInvalidation(const ObjectId& id, size_t n) {
    fake_delegate_.AcknowledgeNthInvalidation(id, n);
  }

  void DropNthInvalidation(const ObjectId& id, size_t n) {
    return fake_delegate_.DropNthInvalidation(id, n);
  }

  void RecoverFromDropEvent(const ObjectId& id) {
    return fake_delegate_.RecoverFromDropEvent(id);
  }

  InvalidatorState GetInvalidatorState() {
    return fake_delegate_.GetInvalidatorState();
  }

  void AcknowledgeAll(const ObjectId& id) { fake_delegate_.AcknowledgeAll(id); }

  ObjectIdSet GetRegisteredIds() const {
    return listener_.GetRegisteredIdsForTest();
  }

  // |payload| can be NULL.
  void FireInvalidate(const ObjectId& object_id,
                      int64_t version,
                      const char* payload) {
    invalidation::Invalidation inv;
    if (payload) {
      inv = invalidation::Invalidation(object_id, version, payload);
    } else {
      inv = invalidation::Invalidation(object_id, version);
    }
    listener_.Invalidate(fake_invalidation_client_, inv);
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const ObjectId& object_id) {
    listener_.InvalidateUnknownVersion(fake_invalidation_client_, object_id);
  }

  void FireInvalidateAll() {
    listener_.InvalidateAll(fake_invalidation_client_);
  }

  void EnableNotifications() {
    fcm_sync_network_channel_->NotifyChannelStateChange(INVALIDATIONS_ENABLED);
    listener_.InformTokenRecieved(fake_invalidation_client_, "token");
  }

  void DisableNotifications(InvalidatorState state) {
    fcm_sync_network_channel_->NotifyChannelStateChange(state);
  }

  const ObjectId kBookmarksId_;
  const ObjectId kPreferencesId_;
  const ObjectId kExtensionsId_;
  const ObjectId kAppsId_;

  ObjectIdSet registered_ids_;

 private:
  base::MessageLoop message_loop_;
  FCMSyncNetworkChannel* fcm_sync_network_channel_;

 protected:
  // A derrived test needs direct access to this.
  FakeInvalidationStateTracker fake_tracker_;

  // Tests need to access these directly.
  FakeInvalidationClient* fake_invalidation_client_;
  FCMSyncInvalidationListener listener_;

 private:
  FakeDelegate fake_delegate_;
};

// Invalidation tests.

// Fire an invalidation without a payload.  It should be processed,
// the payload should remain empty, and the version should be updated.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateNoPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, nullptr);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ("", GetPayload(id));
}

// Fire an invalidation with an empty payload.  It should be
// processed, the payload should remain empty, and the version should
// be updated.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateEmptyPayload) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidate(id, kVersion1, "");

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ("", GetPayload(id));
}

// Fire an invalidation with a payload.  It should be processed, and
// both the payload and the version should be updated.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateWithPayload) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire ten invalidations in a row.  All should be received.
TEST_F(FCMSyncInvalidationListenerTest, ManyInvalidations_NoDrop) {
  const int kRepeatCount = 10;
  const ObjectId& id = kPreferencesId_;
  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(id, i, kPayload1);
  }
  ASSERT_EQ(static_cast<size_t>(kRepeatCount), GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(id));
}

// Fire an invalidation for an unregistered object ID with a payload.  It should
// still be processed, and both the payload and the version should be updated.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateBeforeRegistration_Simple) {
  const ObjectId kUnregisteredId(kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;
  ObjectIdSet ids;
  ids.insert(id);

  EXPECT_EQ(0U, GetInvalidationCount(id));

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(0U, GetInvalidationCount(id));

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredIds(ids);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire ten invalidations before an object registers.  Some invalidations will
// be dropped an replaced with an unknown version invalidation.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateBeforeRegistration_Drop) {
  const int kRepeatCount =
      UnackedInvalidationSet::kMaxBufferedInvalidations + 1;
  const ObjectId kUnregisteredId(kChromeSyncSourceId, "unregistered");
  const ObjectId& id = kUnregisteredId;
  ObjectIdSet ids;
  ids.insert(id);

  EXPECT_EQ(0U, GetInvalidationCount(id));

  int64_t initial_version = kVersion1;
  for (int64_t i = initial_version; i < initial_version + kRepeatCount; ++i) {
    FireInvalidate(id, i, kPayload1);
  }

  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);
  listener_.UpdateRegisteredIds(ids);

  ASSERT_EQ(UnackedInvalidationSet::kMaxBufferedInvalidations,
            GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(initial_version + kRepeatCount - 1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
  EXPECT_TRUE(StartsWithUnknownVersion(id));
}

// Fire an invalidation, then fire another one with a lower version.  Both
// should be received.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateVersion) {
  const ObjectId& id = kPreferencesId_;

  FireInvalidate(id, kVersion2, kPayload2);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));
  EXPECT_EQ(kVersion2, GetVersion(id));
  EXPECT_EQ(kPayload2, GetPayload(id));

  FireInvalidate(id, kVersion1, kPayload1);

  ASSERT_EQ(2U, GetInvalidationCount(id));
  ASSERT_FALSE(IsUnknownVersion(id));

  EXPECT_EQ(kVersion1, GetVersion(id));
  EXPECT_EQ(kPayload1, GetPayload(id));
}

// Fire an invalidation with an unknown version.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateUnknownVersion) {
  const ObjectId& id = kBookmarksId_;

  FireInvalidateUnknownVersion(id);

  ASSERT_EQ(1U, GetInvalidationCount(id));
  EXPECT_TRUE(IsUnknownVersion(id));
}

// Fire an invalidation for all enabled IDs.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateAll) {
  FireInvalidateAll();

  for (ObjectIdSet::const_iterator it = registered_ids_.begin();
       it != registered_ids_.end(); ++it) {
    ASSERT_EQ(1U, GetInvalidationCount(*it));
    EXPECT_TRUE(IsUnknownVersion(*it));
  }
}

// Test a simple scenario for multiple IDs.
TEST_F(FCMSyncInvalidationListenerTest, InvalidateMultipleIds) {
  FireInvalidate(kBookmarksId_, 3, nullptr);

  ASSERT_EQ(1U, GetInvalidationCount(kBookmarksId_));
  ASSERT_FALSE(IsUnknownVersion(kBookmarksId_));
  EXPECT_EQ(3, GetVersion(kBookmarksId_));
  EXPECT_EQ("", GetPayload(kBookmarksId_));

  // kExtensionId is not registered, so the invalidation should not get through.
  FireInvalidate(kExtensionsId_, 2, nullptr);
  ASSERT_EQ(0U, GetInvalidationCount(kExtensionsId_));
}

// Without readying the client, disable notifications, then enable
// them.  The listener should still think notifications are disabled.
TEST_F(FCMSyncInvalidationListenerTest, EnableNotificationsNotReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  DisableNotifications(TRANSIENT_INVALIDATION_ERROR);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());
}

// Enable notifications then Ready the invalidation client.  The
// delegate should then be ready.
TEST_F(FCMSyncInvalidationListenerTest, EnableNotificationsThenReady) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Ready the invalidation client then enable notifications.  The
// delegate should then be ready.
TEST_F(FCMSyncInvalidationListenerTest, ReadyThenEnableNotifications) {
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// Enable notifications and ready the client.  Then disable
// notifications with an auth error and re-enable notifications.  The
// delegate should go into an auth error mode and then back out.
TEST_F(FCMSyncInvalidationListenerTest, PushClientAuthError) {
  EnableNotifications();
  listener_.Ready(fake_invalidation_client_);

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());

  DisableNotifications(INVALIDATION_CREDENTIALS_REJECTED);

  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, GetInvalidatorState());

  EnableNotifications();

  EXPECT_EQ(INVALIDATIONS_ENABLED, GetInvalidatorState());
}

// A variant of FCMSyncInvalidationListenerTest that starts with some initial
// state.  We make not attempt to abstract away the contents of this state.  The
// tests that make use of this harness depend on its implementation details.
class FCMSyncInvalidationListenerTest_WithInitialState
    : public FCMSyncInvalidationListenerTest {
 public:
  void SetUp() override {
    UnackedInvalidationSet bm_state(kBookmarksId_);
    UnackedInvalidationSet ext_state(kExtensionsId_);

    Invalidation bm_unknown = Invalidation::InitUnknownVersion(kBookmarksId_);
    Invalidation bm_v100 = Invalidation::Init(kBookmarksId_, 100, "hundred");
    bm_state.Add(bm_unknown);
    bm_state.Add(bm_v100);

    Invalidation ext_v10 = Invalidation::Init(kExtensionsId_, 10, "ten");
    Invalidation ext_v20 = Invalidation::Init(kExtensionsId_, 20, "twenty");
    ext_state.Add(ext_v10);
    ext_state.Add(ext_v20);

    initial_state.insert(std::make_pair(kBookmarksId_, bm_state));
    initial_state.insert(std::make_pair(kExtensionsId_, ext_state));

    fake_tracker_.SetSavedInvalidations(initial_state);

    FCMSyncInvalidationListenerTest::SetUp();
  }

  UnackedInvalidationsMap initial_state;
};

}  // namespace

}  // namespace syncer
