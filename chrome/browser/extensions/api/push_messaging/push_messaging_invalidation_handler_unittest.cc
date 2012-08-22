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
using ::testing::InSequence;
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
               void(syncer::SyncNotifierObserver*));
  MOCK_METHOD2(UpdateRegisteredInvalidationIds,
               void(syncer::SyncNotifierObserver*, const syncer::ObjectIdSet&));
  MOCK_METHOD1(UnregisterInvalidationHandler,
               void(syncer::SyncNotifierObserver*));

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
    SetUpWithArgs(std::set<std::string>(), syncer::ObjectIdSet());
  }

  virtual void SetUpWithArgs(const std::set<std::string>& extension_ids,
                             const syncer::ObjectIdSet& expected_ids) {
    InSequence seq;
    syncer::SyncNotifierObserver* handler[2] = {};
    EXPECT_CALL(service_, RegisterInvalidationHandler(NotNull()))
        .WillOnce(SaveArg<0>(&handler[0]));
    EXPECT_CALL(service_,
                UpdateRegisteredInvalidationIds(NotNull(), expected_ids))
        .WillOnce(SaveArg<0>(&handler[1]));
    handler_.reset(new PushMessagingInvalidationHandler(
        &service_, &delegate_, extension_ids));
    EXPECT_EQ(handler[0], handler[1]);
    EXPECT_EQ(handler_.get(), handler[0]);

  }
  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(service_, UnregisterInvalidationHandler(handler_.get()));
    handler_.reset();
  }
  StrictMock<MockInvalidationFrontend> service_;
  StrictMock<MockInvalidationHandlerDelegate> delegate_;
  scoped_ptr<PushMessagingInvalidationHandler> handler_;
};

// Tests that we correctly register any extensions passed in when constructed.
TEST_F(PushMessagingInvalidationHandlerTest, Construction) {
  TearDown();

  InSequence seq;
  std::set<std::string> extension_ids;
  extension_ids.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  extension_ids.insert("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  syncer::ObjectIdSet expected_ids;
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/0"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/1"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/2"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/3"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/0"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/1"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/2"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/3"));

  SetUpWithArgs(extension_ids, expected_ids);
}

TEST_F(PushMessagingInvalidationHandlerTest, RegisterUnregisterExtension) {
  syncer::ObjectIdSet expected_ids;
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/cccccccccccccccccccccccccccccccc/0"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/cccccccccccccccccccccccccccccccc/1"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/cccccccccccccccccccccccccccccccc/2"));
  expected_ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
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
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/dddddddddddddddddddddddddddddddd/0"));
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/dddddddddddddddddddddddddddddddd/3"));
  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 0, "payload"));
  EXPECT_CALL(delegate_,
              OnMessage("dddddddddddddddddddddddddddddddd", 3, "payload"));
  handler_->OnIncomingNotification(ObjectIdSetToPayloadMap(ids, "payload"),
                                   syncer::REMOTE_NOTIFICATION);
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
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "V/dddddddddddddddddddddddddddddddd/3"));
  // Invalid extension ID length.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/ddddddddddddddddddddddddddddddddd/3"));
  // Non-numeric subchannel.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/dddddddddddddddddddddddddddddddd/z"));
  // Subchannel out of range.
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      "U/dddddddddddddddddddddddddddddddd/4"));
  handler_->OnIncomingNotification(ObjectIdSetToPayloadMap(ids, "payload"),
                                   syncer::REMOTE_NOTIFICATION);
}

}  // namespace extensions
