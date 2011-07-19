// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

namespace jingle_glue {

static const uint32 kTestMessage1 = 1;
static const uint32 kTestMessage2 = 2;

static const int kTestDelayMs1 = 10;
static const int kTestDelayMs2 = 20;
static const int kTestDelayMs3 = 30;
static const int kTestDelayMs4 = 40;
static const int kMaxTestDelay = 40;

class MockMessageHandler : public talk_base::MessageHandler {
 public:
  MOCK_METHOD1(OnMessage, void(talk_base::Message* msg));
};

class ThreadWrapperTest : public testing::Test {
 protected:
  ThreadWrapperTest() {
  }

  talk_base::Thread* thread() {
    return talk_base::Thread::Current();
  }

  virtual void SetUp() OVERRIDE {
    JingleThreadWrapper::EnsureForCurrentThread();
  }

  // ThreadWrapper destroyes itself when |message_loop_| is destroyed.
  MessageLoop message_loop_;
  MockMessageHandler handler1_;
  MockMessageHandler handler2_;
};

MATCHER_P3(MatchMessage, handler, message_id, data, "") {
  return arg->phandler == handler &&
      arg->message_id == message_id &&
      arg->pdata == data;
}

ACTION(DeleteMessageData) {
  delete arg0->pdata;
}

TEST_F(ThreadWrapperTest, Post) {
  talk_base::MessageData* data1_ = new talk_base::MessageData();
  talk_base::MessageData* data2_ = new talk_base::MessageData();
  talk_base::MessageData* data3_ = new talk_base::MessageData();
  talk_base::MessageData* data4_ = new talk_base::MessageData();

  thread()->Post(&handler1_, kTestMessage1, data1_);
  thread()->Post(&handler1_, kTestMessage2, data2_);
  thread()->Post(&handler2_, kTestMessage1, data3_);
  thread()->Post(&handler2_, kTestMessage1, data4_);

  InSequence in_seq;

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data1_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage2, data2_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data3_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data4_)))
      .WillOnce(DeleteMessageData());

  message_loop_.RunAllPending();
}

TEST_F(ThreadWrapperTest, PostDelayed) {
  talk_base::MessageData* data1_ = new talk_base::MessageData();
  talk_base::MessageData* data2_ = new talk_base::MessageData();
  talk_base::MessageData* data3_ = new talk_base::MessageData();
  talk_base::MessageData* data4_ = new talk_base::MessageData();

  thread()->PostDelayed(kTestDelayMs1, &handler1_, kTestMessage1, data1_);
  thread()->PostDelayed(kTestDelayMs2, &handler1_, kTestMessage2, data2_);
  thread()->PostDelayed(kTestDelayMs3, &handler2_, kTestMessage1, data3_);
  thread()->PostDelayed(kTestDelayMs4, &handler2_, kTestMessage1, data4_);

  InSequence in_seq;

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data1_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage2, data2_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data3_)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data4_)))
      .WillOnce(DeleteMessageData());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                kMaxTestDelay);
  message_loop_.Run();
}

TEST_F(ThreadWrapperTest, Clear) {
  thread()->Post(&handler1_, kTestMessage1, NULL);
  thread()->Post(&handler1_, kTestMessage2, NULL);
  thread()->Post(&handler2_, kTestMessage1, NULL);
  thread()->Post(&handler2_, kTestMessage2, NULL);

  thread()->Clear(&handler1_, kTestMessage2);

  InSequence in_seq;

  talk_base::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage2, null_data)))
      .WillOnce(DeleteMessageData());

  message_loop_.RunAllPending();
}

TEST_F(ThreadWrapperTest, ClearDelayed) {
  thread()->PostDelayed(kTestDelayMs1, &handler1_, kTestMessage1, NULL);
  thread()->PostDelayed(kTestDelayMs2, &handler1_, kTestMessage2, NULL);
  thread()->PostDelayed(kTestDelayMs3, &handler2_, kTestMessage1, NULL);
  thread()->PostDelayed(kTestDelayMs4, &handler2_, kTestMessage1, NULL);

  thread()->Clear(&handler1_, kTestMessage2);

  InSequence in_seq;

  talk_base::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                kMaxTestDelay);
  message_loop_.Run();
}

// Verify that the queue is cleared when a handler is destroyed.
TEST_F(ThreadWrapperTest, ClearDestoroyed) {
  MockMessageHandler* handler_ptr;
  {
    MockMessageHandler handler;
    handler_ptr = &handler;
    thread()->Post(&handler, kTestMessage1, NULL);
  }
  talk_base::MessageList removed;
  thread()->Clear(handler_ptr, talk_base::MQID_ANY, &removed);
  DCHECK_EQ(0U, removed.size());
}

}  // namespace jingle_glue
