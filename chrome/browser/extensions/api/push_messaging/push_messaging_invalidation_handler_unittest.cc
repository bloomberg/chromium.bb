// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "chrome/browser/sync/invalidation_frontend.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace extensions {

namespace {

class MockInvalidationFrontend : public InvalidationFrontend {
 public:
  MockInvalidationFrontend();
  ~MockInvalidationFrontend();
  MOCK_METHOD1(RegisterInvalidationHandler,
               void(syncer::InvalidationHandler*));
  MOCK_METHOD2(UpdateRegisteredInvalidationIds,
               void(syncer::InvalidationHandler*, const syncer::ObjectIdSet&));
  MOCK_METHOD1(UnregisterInvalidationHandler,
               void(syncer::InvalidationHandler*));
  MOCK_CONST_METHOD0(GetInvalidatorState, syncer::InvalidatorState());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInvalidationFrontend);
};

MockInvalidationFrontend::MockInvalidationFrontend() {}
MockInvalidationFrontend::~MockInvalidationFrontend() {}

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
  StrictMock<MockInvalidationFrontend> service_;
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
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/0"));
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/3"));
  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 0, "payload"));
  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 3, "payload"));
  handler_->OnIncomingInvalidation(ObjectIdSetToInvalidationMap(ids, "payload"),
                                   syncer::REMOTE_INVALIDATION);
}

// Tests that malformed object IDs don't trigger spurious callbacks.
TEST_F(PushMessagingInvalidationHandlerTest, DispatchInvalidObjectIds) {
  syncer::ObjectIdSet ids;
  // Completely incorrect format.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::TEST,
      "Invalid"));
  // Incorrect source.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::TEST,
      "U/dddddddddddddddddddddddddddddddd/3"));
  // Incorrect format type.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "V/dddddddddddddddddddddddddddddddd/3"));
  // Invalid extension ID length.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/ddddddddddddddddddddddddddddddddd/3"));
  // Non-numeric subchannel.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/z"));
  // Subchannel out of range.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      "U/dddddddddddddddddddddddddddddddd/4"));
  handler_->OnIncomingInvalidation(ObjectIdSetToInvalidationMap(ids, "payload"),
                                   syncer::REMOTE_INVALIDATION);
}

}  // namespace extensions
