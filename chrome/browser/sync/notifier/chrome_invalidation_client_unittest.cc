// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "google/cacheinvalidation/v2/invalidation-client.h"
#include "google/cacheinvalidation/v2/types.h"
#include "google/cacheinvalidation/v2/types.pb.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

const char kClientId[] = "client_id";
const char kClientInfo[] = "client_info";
const char kState[] = "state";

class MockInvalidationClient : public invalidation::InvalidationClient {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(Register, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Register, void(const std::vector<invalidation::ObjectId>&));
  MOCK_METHOD1(Unregister, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Unregister, void(const std::vector<invalidation::ObjectId>&));
  MOCK_METHOD1(Acknowledge, void(const invalidation::AckHandle&));
};

class MockListener : public ChromeInvalidationClient::Listener {
 public:
  MOCK_METHOD1(OnInvalidate, void(const syncable::ModelTypePayloadMap&));
  MOCK_METHOD1(OnSessionStatusChanged, void(bool));
};

class MockInvalidationVersionTracker
    : public InvalidationVersionTracker,
      public base::SupportsWeakPtr<MockInvalidationVersionTracker> {
 public:
  MOCK_CONST_METHOD0(GetAllMaxVersions, InvalidationVersionMap());
  MOCK_METHOD2(SetMaxVersion, void(syncable::ModelType, int64));
};

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

class MockCallback {
 public:
  MOCK_METHOD0(Run, void());

  invalidation::Closure* MakeClosure() {
    return invalidation::NewPermanentCallback(this, &MockCallback::Run);
  }
};

}  // namespace

class ChromeInvalidationClientTest : public testing::Test {
 protected:
  virtual void SetUp() {
    client_.Start(kClientId, kClientInfo, kState,
                  InvalidationVersionMap(),
                  browser_sync::MakeWeakHandle(
                      mock_invalidation_version_tracker_.AsWeakPtr()),
                  &mock_listener_, &mock_state_writer_,
                  fake_base_task_.AsWeakPtr());
  }

  virtual void TearDown() {
    client_.Stop();
    message_loop_.RunAllPending();
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidate(const char* type_name,
                      int64 version, const char* payload) {
    const invalidation::ObjectId object_id(
        ipc::invalidation::ObjectSource::CHROME_SYNC, type_name);
    std::string payload_tmp = payload ? payload : "";
    invalidation::Invalidation inv;
    if (payload) {
      inv = invalidation::Invalidation(object_id, version, payload);
    } else {
      inv = invalidation::Invalidation(object_id, version);
    }
    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.Invalidate(&mock_invalidation_client_, inv, ack_handle);
    // Pump message loop to trigger
    // InvalidationVersionTracker::SetMaxVersion().
    message_loop_.RunAllPending();
  }

  // |payload| can be NULL, but not |type_name|.
  void FireInvalidateUnknownVersion(const char* type_name) {
    const invalidation::ObjectId object_id(
        ipc::invalidation::ObjectSource::CHROME_SYNC, type_name);

    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.InvalidateUnknownVersion(&mock_invalidation_client_, object_id,
                                     ack_handle);
  }

  void FireInvalidateAll() {
    invalidation::AckHandle ack_handle("fakedata");
    EXPECT_CALL(mock_invalidation_client_, Acknowledge(ack_handle));
    client_.InvalidateAll(&mock_invalidation_client_, ack_handle);
  }

  MessageLoop message_loop_;
  StrictMock<MockListener> mock_listener_;
  StrictMock<MockInvalidationVersionTracker>
      mock_invalidation_version_tracker_;
  StrictMock<MockStateWriter> mock_state_writer_;
  StrictMock<MockInvalidationClient> mock_invalidation_client_;
  notifier::FakeBaseTask fake_base_task_;
  ChromeInvalidationClient client_;
};

namespace {

syncable::ModelTypePayloadMap MakeMap(syncable::ModelType model_type,
                                      const std::string& payload) {
  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[model_type] = payload;
  return type_payloads;
}

syncable::ModelTypePayloadMap MakeMapFromSet(syncable::ModelTypeSet types,
                                             const std::string& payload) {
  syncable::ModelTypePayloadMap type_payloads;
  for (syncable::ModelTypeSet::const_iterator it = types.begin();
       it != types.end(); ++it) {
    type_payloads[*it] = payload;
  }
  return type_payloads;
}

}  // namespace

TEST_F(ChromeInvalidationClientTest, InvalidateBadObjectId) {
  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::APPS);
  client_.RegisterTypes(types);
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidate("bad", 1, NULL);
  message_loop_.RunAllPending();
}

TEST_F(ChromeInvalidationClientTest, InvalidateNoPayload) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::BOOKMARKS, "")));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::BOOKMARKS, 1));
  FireInvalidate("BOOKMARK", 1, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateWithPayload) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::PREFERENCES, "payload")));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::PREFERENCES, 1));
  FireInvalidate("PREFERENCE", 1, "payload");
}

TEST_F(ChromeInvalidationClientTest, InvalidateVersion) {
  using ::testing::Mock;

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::APPS, 1));

  // Should trigger.
  FireInvalidate("APP", 1, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);

  // Should be dropped.
  FireInvalidate("APP", 1, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateUnknownVersion) {
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")))
      .Times(2);

  // Should trigger twice.
  FireInvalidateUnknownVersion("EXTENSION");
  FireInvalidateUnknownVersion("EXTENSION");
}

TEST_F(ChromeInvalidationClientTest, InvalidateVersionMultipleTypes) {
  using ::testing::Mock;

  syncable::ModelTypeSet types;
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::APPS);
  client_.RegisterTypes(types);

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")));

  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::APPS, 3));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::EXTENSIONS, 2));

  // Should trigger both.
  FireInvalidate("APP", 3, NULL);
  FireInvalidate("EXTENSION", 2, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_version_tracker_);

  // Should both be dropped.
  FireInvalidate("APP", 1, NULL);
  FireInvalidate("EXTENSION", 1, NULL);

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_version_tracker_);

  // InvalidateAll shouldn't change any version state.
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();

  Mock::VerifyAndClearExpectations(&mock_listener_);
  Mock::VerifyAndClearExpectations(&mock_invalidation_version_tracker_);

  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::PREFERENCES, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::EXTENSIONS, "")));
  EXPECT_CALL(mock_listener_,
              OnInvalidate(MakeMap(syncable::APPS, "")));

  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::PREFERENCES, 5));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::EXTENSIONS, 3));
  EXPECT_CALL(mock_invalidation_version_tracker_,
              SetMaxVersion(syncable::APPS, 4));

  // Should trigger all three.
  FireInvalidate("PREFERENCE", 5, NULL);
  FireInvalidate("EXTENSION", 3, NULL);
  FireInvalidate("APP", 4, NULL);
}

TEST_F(ChromeInvalidationClientTest, InvalidateAll) {
  syncable::ModelTypeSet types;
  types.insert(syncable::PREFERENCES);
  types.insert(syncable::EXTENSIONS);
  client_.RegisterTypes(types);
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();
}

TEST_F(ChromeInvalidationClientTest, RegisterTypes) {
  syncable::ModelTypeSet types;
  types.insert(syncable::PREFERENCES);
  types.insert(syncable::EXTENSIONS);
  client_.RegisterTypes(types);
  // Registered types should be preserved across Stop/Start.
  TearDown();
  SetUp();
  EXPECT_CALL(mock_listener_, OnInvalidate(MakeMapFromSet(types, "")));
  FireInvalidateAll();
}

// TODO(akalin): Flesh out unit tests.

}  // namespace sync_notifier
