// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/mediator_thread_impl.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "net/base/capturing_net_log.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::StrictMock;

// TODO(sanjeevr): Move this to net_test_support.
// Used to return a dummy context.
class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestURLRequestContextGetter()
      : message_loop_proxy_(base::MessageLoopProxy::current()) {
  }
  virtual ~TestURLRequestContextGetter() { }

  // net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return message_loop_proxy_;
  }

 private:
  scoped_refptr<net::URLRequestContext> context_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

class MockObserver : public MediatorThread::Observer {
 public:
  MOCK_METHOD1(OnConnectionStateChange, void(bool));
  MOCK_METHOD1(OnSubscriptionStateChange, void(bool));
  MOCK_METHOD1(OnIncomingNotification, void(const Notification&));
  MOCK_METHOD0(OnOutgoingNotification, void());
};

}  // namespace

class MediatorThreadTest : public testing::Test {
 protected:
  MediatorThreadTest() {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter();
  }

  virtual ~MediatorThreadTest() {}

  virtual void SetUp() {
    mediator_thread_.reset(new MediatorThreadImpl(notifier_options_));
    mediator_thread_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    // Clear out any messages posted by MediatorThread's
    // destructor.
    message_loop_.RunAllPending();
    mediator_thread_->RemoveObserver(&mock_observer_);
    mediator_thread_.reset();
  }

  // Needed by TestURLRequestContextGetter.
  MessageLoopForIO message_loop_;
  NotifierOptions notifier_options_;
  StrictMock<MockObserver> mock_observer_;
  scoped_ptr<MediatorThreadImpl> mediator_thread_;
  FakeBaseTask fake_base_task_;
};

TEST_F(MediatorThreadTest, SendNotificationBasic) {
  EXPECT_CALL(mock_observer_, OnConnectionStateChange(true));
  EXPECT_CALL(mock_observer_, OnOutgoingNotification());

  mediator_thread_->TriggerOnConnectForTest(fake_base_task_.AsWeakPtr());
  mediator_thread_->SendNotification(Notification());
  mediator_thread_->Logout();

  // Shouldn't trigger.
  mediator_thread_->SendNotification(Notification());
}

TEST_F(MediatorThreadTest, SendNotificationDelayed) {
  EXPECT_CALL(mock_observer_, OnConnectionStateChange(true));
  EXPECT_CALL(mock_observer_, OnOutgoingNotification()).Times(5);

  for (int i = 0; i < 5; ++i) {
    mediator_thread_->SendNotification(Notification());
  }
  mediator_thread_->TriggerOnConnectForTest(fake_base_task_.AsWeakPtr());
}

TEST_F(MediatorThreadTest, SendNotificationDelayedTwice) {
  EXPECT_CALL(mock_observer_, OnConnectionStateChange(true)).Times(2);
  EXPECT_CALL(mock_observer_, OnOutgoingNotification()).Times(5);

  for (int i = 0; i < 5; ++i) {
    mediator_thread_->SendNotification(Notification());
  }
  mediator_thread_->TriggerOnConnectForTest(
      base::WeakPtr<buzz::XmppTaskParentInterface>());
  mediator_thread_->TriggerOnConnectForTest(fake_base_task_.AsWeakPtr());
}

}  // namespace notifier
