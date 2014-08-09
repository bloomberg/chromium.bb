// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "components/invalidation/invalidation_logger.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;

namespace extensions {

namespace {

class MockInvalidationService : public invalidation::InvalidationService {
 public:
  MockInvalidationService();
  ~MockInvalidationService();
  MOCK_METHOD1(RegisterInvalidationHandler,
               void(syncer::InvalidationHandler*));
  MOCK_METHOD2(UpdateRegisteredInvalidationIds,
               void(syncer::InvalidationHandler*, const syncer::ObjectIdSet&));
  MOCK_METHOD1(UnregisterInvalidationHandler,
               void(syncer::InvalidationHandler*));
  MOCK_CONST_METHOD0(GetInvalidatorState, syncer::InvalidatorState());
  MOCK_CONST_METHOD0(GetInvalidatorClientId, std::string());
  MOCK_METHOD0(GetInvalidationLogger, invalidation::InvalidationLogger*());
  MOCK_CONST_METHOD1(RequestDetailedStatus,
                     void(base::Callback<void(const base::DictionaryValue&)>));
  MOCK_METHOD0(GetIdentityProvider, IdentityProvider*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInvalidationService);
};

MockInvalidationService::MockInvalidationService() {}
MockInvalidationService::~MockInvalidationService() {}

class MockInvalidationHandlerDelegate
    : public PushMessagingInvalidationHandlerDelegate {
 public:
  MockInvalidationHandlerDelegate();
  ~MockInvalidationHandlerDelegate();
  MOCK_METHOD3(OnMessage,
               void(const std::string&, int, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInvalidationHandlerDelegate);
};

MockInvalidationHandlerDelegate::MockInvalidationHandlerDelegate() {}
MockInvalidationHandlerDelegate::~MockInvalidationHandlerDelegate() {}

}  // namespace

class PushMessagingInvalidationHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    syncer::InvalidationHandler* handler = NULL;
    EXPECT_CALL(service_, RegisterInvalidationHandler(NotNull()))
        .WillOnce(SaveArg<0>(&handler));
    handler_.reset(new PushMessagingInvalidationHandler(
        &service_, &delegate_));
    EXPECT_EQ(handler_.get(), handler);
  }
  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(service_, UnregisterInvalidationHandler(handler_.get()));
    handler_.reset();
  }
  StrictMock<MockInvalidationService> service_;
  StrictMock<MockInvalidationHandlerDelegate> delegate_;
  scoped_ptr<PushMessagingInvalidationHandler> handler_;
};

TEST_F(PushMessagingInvalidationHandlerTest, RegisterUnregisterExtension) {
  syncer::ObjectIdSet expected_ids;
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/cccccccccccccccccccccccccccccccc/0"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/cccccccccccccccccccccccccccccccc/1"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/cccccccccccccccccccccccccccccccc/2"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/cccccccccccccccccccccccccccccccc/3"));
  EXPECT_CALL(service_,
              UpdateRegisteredInvalidationIds(handler_.get(), expected_ids));
  handler_->RegisterExtension("cccccccccccccccccccccccccccccccc");
  EXPECT_CALL(service_,
              UpdateRegisteredInvalidationIds(handler_.get(),
                                              syncer::ObjectIdSet()));
  handler_->UnregisterExtension("cccccccccccccccccccccccccccccccc");
}

TEST_F(PushMessagingInvalidationHandlerTest, Dispatch) {
  syncer::ObjectIdInvalidationMap invalidation_map;
  // A normal invalidation.
  invalidation_map.Insert(
      syncer::Invalidation::Init(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
              "U/dddddddddddddddddddddddddddddddd/0"),
          10,
          "payload"));

  // An unknown version invalidation.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
      invalidation::ObjectId(
        ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
        "U/dddddddddddddddddddddddddddddddd/3")));

  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 0, "payload"));
  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 3, ""));
  handler_->OnIncomingInvalidation(invalidation_map);
}

// Tests that malformed object IDs don't trigger spurious callbacks.
TEST_F(PushMessagingInvalidationHandlerTest, DispatchInvalidObjectIds) {
  syncer::ObjectIdInvalidationMap invalidation_map;
  // Completely incorrect format.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::TEST,
              "Invalid")));
  // Incorrect source.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::TEST,
              "U/dddddddddddddddddddddddddddddddd/3")));
  // Incorrect format type.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
              "V/dddddddddddddddddddddddddddddddd/3")));
  // Invalid extension ID length.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
              "U/ddddddddddddddddddddddddddddddddd/3")));
  // Non-numeric subchannel.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
              "U/dddddddddddddddddddddddddddddddd/z")));
  // Subchannel out of range.
  invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(
          invalidation::ObjectId(
              ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
              "U/dddddddddddddddddddddddddddddddd/4")));
  handler_->OnIncomingInvalidation(invalidation_map);
}

// Test version filtering of incoming invalidations.
TEST_F(PushMessagingInvalidationHandlerTest, InvalidationVersionsOutOfOrder) {
  const invalidation::ObjectId id0(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/0");
  const invalidation::ObjectId id3(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/3");

  // The first received invalidation should get through.
  syncer::ObjectIdInvalidationMap map1;
  map1.Insert(syncer::Invalidation::Init(id0, 5, "5"));
  EXPECT_CALL(delegate_, OnMessage("dddddddddddddddddddddddddddddddd", 0, "5"));
  handler_->OnIncomingInvalidation(map1);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Invalid versions are always allowed through.
  syncer::ObjectIdInvalidationMap map2;
  map2.Insert(syncer::Invalidation::InitUnknownVersion(id0));
  EXPECT_CALL(delegate_, OnMessage("dddddddddddddddddddddddddddddddd", 0, ""));
  handler_->OnIncomingInvalidation(map2);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // An older version should not make it through.
  syncer::ObjectIdInvalidationMap map3;
  map3.Insert(syncer::Invalidation::Init(id0, 4, "4"));
  handler_->OnIncomingInvalidation(map3);

  // A newer version will make it through.
  syncer::ObjectIdInvalidationMap map4;
  map4.Insert(syncer::Invalidation::Init(id0, 6, "6"));
  EXPECT_CALL(delegate_, OnMessage("dddddddddddddddddddddddddddddddd", 0, "6"));
  handler_->OnIncomingInvalidation(map4);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // An unrelated object should be unaffected by all the above.
  syncer::ObjectIdInvalidationMap map5;
  map5.Insert(syncer::Invalidation::Init(id3, 1, "1"));
  EXPECT_CALL(delegate_, OnMessage("dddddddddddddddddddddddddddddddd", 3, "1"));
  handler_->OnIncomingInvalidation(map5);
  testing::Mock::VerifyAndClearExpectations(&delegate_);
}

}  // namespace extensions
