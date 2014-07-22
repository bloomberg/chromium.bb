// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FrameSwapMessageQueueTest : public testing::Test {
 public:
  FrameSwapMessageQueueTest()
      : first_message_(41, 1, IPC::Message::PRIORITY_NORMAL),
        second_message_(42, 2, IPC::Message::PRIORITY_NORMAL),
        third_message_(43, 3, IPC::Message::PRIORITY_NORMAL),
        queue_(new FrameSwapMessageQueue()) {}

 protected:
  void QueueNextSwapMessage(scoped_ptr<IPC::Message> msg) {
    queue_->QueueMessageForFrame(
        MESSAGE_DELIVERY_POLICY_WITH_NEXT_SWAP, 0, msg.Pass(), NULL);
  }

  void QueueNextSwapMessage(scoped_ptr<IPC::Message> msg, bool* first) {
    queue_->QueueMessageForFrame(
        MESSAGE_DELIVERY_POLICY_WITH_NEXT_SWAP, 0, msg.Pass(), first);
  }

  void QueueVisualStateMessage(int source_frame_number,
                               scoped_ptr<IPC::Message> msg) {
    queue_->QueueMessageForFrame(MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE,
                                 source_frame_number,
                                 msg.Pass(),
                                 NULL);
  }

  void QueueVisualStateMessage(int source_frame_number,
                               scoped_ptr<IPC::Message> msg,
                               bool* first) {
    queue_->QueueMessageForFrame(MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE,
                                 source_frame_number,
                                 msg.Pass(),
                                 first);
  }

  void DrainMessages(int source_frame_number,
                     ScopedVector<IPC::Message>* messages) {
    messages->clear();
    queue_->DidSwap(source_frame_number);
    scoped_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
        queue_->AcquireSendMessageScope();
    queue_->DrainMessages(messages);
  }

  bool HasMessageForId(const ScopedVector<IPC::Message>& messages,
                       int routing_id) {
    for (ScopedVector<IPC::Message>::const_iterator i = messages.begin();
         i != messages.end();
         ++i) {
      if ((*i)->routing_id() == routing_id)
        return true;
    }
    return false;
  }

  scoped_ptr<IPC::Message> CloneMessage(const IPC::Message& other) {
    return make_scoped_ptr(new IPC::Message(other)).Pass();
  }

  void TestDidNotSwap(cc::SwapPromise::DidNotSwapReason reason);

  IPC::Message first_message_;
  IPC::Message second_message_;
  IPC::Message third_message_;
  scoped_refptr<FrameSwapMessageQueue> queue_;
};

TEST_F(FrameSwapMessageQueueTest, TestEmptyQueueDrain) {
  ScopedVector<IPC::Message> messages;

  DrainMessages(0, &messages);
  ASSERT_TRUE(messages.empty());
}

TEST_F(FrameSwapMessageQueueTest, TestEmpty) {
  ScopedVector<IPC::Message> messages;
  ASSERT_TRUE(queue_->Empty());
  QueueNextSwapMessage(CloneMessage(first_message_));
  ASSERT_FALSE(queue_->Empty());
  DrainMessages(0, &messages);
  ASSERT_TRUE(queue_->Empty());
  QueueVisualStateMessage(1, CloneMessage(first_message_));
  ASSERT_FALSE(queue_->Empty());
  queue_->DidSwap(1);
  ASSERT_FALSE(queue_->Empty());
}

TEST_F(FrameSwapMessageQueueTest, TestQueueMessageFirst) {
  ScopedVector<IPC::Message> messages;
  bool visual_state_first = false;
  bool next_swap_first = false;

  // Queuing the first time should result in true.
  QueueVisualStateMessage(1, CloneMessage(first_message_), &visual_state_first);
  ASSERT_TRUE(visual_state_first);
  // Queuing the second time should result in true.
  QueueVisualStateMessage(
      1, CloneMessage(second_message_), &visual_state_first);
  ASSERT_FALSE(visual_state_first);
  // Queuing for a different frame should result in true.
  QueueVisualStateMessage(2, CloneMessage(first_message_), &visual_state_first);
  ASSERT_TRUE(visual_state_first);

  // Queuing for a different policy should result in true.
  QueueNextSwapMessage(CloneMessage(first_message_), &next_swap_first);
  ASSERT_TRUE(next_swap_first);
  // Second time for the same policy is still false.
  QueueNextSwapMessage(CloneMessage(first_message_), &next_swap_first);
  ASSERT_FALSE(next_swap_first);

  DrainMessages(4, &messages);
  // Queuing after all messages are drained is a true again.
  QueueVisualStateMessage(4, CloneMessage(first_message_), &visual_state_first);
  ASSERT_TRUE(visual_state_first);
}

TEST_F(FrameSwapMessageQueueTest, TestNextSwapMessageSentWithNextFrame) {
  ScopedVector<IPC::Message> messages;

  DrainMessages(1, &messages);
  QueueNextSwapMessage(CloneMessage(first_message_));
  DrainMessages(2, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_EQ(first_message_.routing_id(), messages.front()->routing_id());
  messages.clear();

  DrainMessages(2, &messages);
  ASSERT_TRUE(messages.empty());
}

TEST_F(FrameSwapMessageQueueTest, TestNextSwapMessageSentWithCurrentFrame) {
  ScopedVector<IPC::Message> messages;

  DrainMessages(1, &messages);
  QueueNextSwapMessage(CloneMessage(first_message_));
  DrainMessages(1, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_EQ(first_message_.routing_id(), messages.front()->routing_id());
  messages.clear();

  DrainMessages(1, &messages);
  ASSERT_TRUE(messages.empty());
}

TEST_F(FrameSwapMessageQueueTest,
       TestDrainsVisualStateMessagesForCorrespondingFrames) {
  ScopedVector<IPC::Message> messages;

  QueueVisualStateMessage(1, CloneMessage(first_message_));
  QueueVisualStateMessage(2, CloneMessage(second_message_));
  QueueVisualStateMessage(3, CloneMessage(third_message_));
  DrainMessages(0, &messages);
  ASSERT_TRUE(messages.empty());

  DrainMessages(2, &messages);
  ASSERT_EQ(2u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, first_message_.routing_id()));
  ASSERT_TRUE(HasMessageForId(messages, second_message_.routing_id()));
  messages.clear();

  DrainMessages(2, &messages);
  ASSERT_TRUE(messages.empty());

  DrainMessages(5, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_EQ(third_message_.routing_id(), messages.front()->routing_id());
}

TEST_F(FrameSwapMessageQueueTest,
       TestQueueNextSwapMessagePreservesFifoOrdering) {
  ScopedVector<IPC::Message> messages;

  QueueNextSwapMessage(CloneMessage(first_message_));
  QueueNextSwapMessage(CloneMessage(second_message_));
  DrainMessages(1, &messages);
  ASSERT_EQ(2u, messages.size());
  ASSERT_EQ(first_message_.routing_id(), messages[0]->routing_id());
  ASSERT_EQ(second_message_.routing_id(), messages[1]->routing_id());
}

TEST_F(FrameSwapMessageQueueTest,
       TestQueueVisualStateMessagePreservesFifoOrdering) {
  ScopedVector<IPC::Message> messages;

  QueueVisualStateMessage(1, CloneMessage(first_message_));
  QueueVisualStateMessage(1, CloneMessage(second_message_));
  DrainMessages(1, &messages);
  ASSERT_EQ(2u, messages.size());
  ASSERT_EQ(first_message_.routing_id(), messages[0]->routing_id());
  ASSERT_EQ(second_message_.routing_id(), messages[1]->routing_id());
}

void FrameSwapMessageQueueTest::TestDidNotSwap(
    cc::SwapPromise::DidNotSwapReason reason) {
  ScopedVector<IPC::Message> messages;

  QueueNextSwapMessage(CloneMessage(first_message_));
  QueueVisualStateMessage(2, CloneMessage(second_message_));
  QueueVisualStateMessage(3, CloneMessage(third_message_));

  queue_->DidNotSwap(2, cc::SwapPromise::COMMIT_NO_UPDATE, &messages);
  ASSERT_EQ(2u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, first_message_.routing_id()));
  ASSERT_TRUE(HasMessageForId(messages, second_message_.routing_id()));
  messages.clear();

  queue_->DidNotSwap(3, cc::SwapPromise::COMMIT_NO_UPDATE, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, third_message_.routing_id()));
  messages.clear();
}

TEST_F(FrameSwapMessageQueueTest, TestDidNotSwapNoUpdate) {
  TestDidNotSwap(cc::SwapPromise::COMMIT_NO_UPDATE);
}

TEST_F(FrameSwapMessageQueueTest, TestDidNotSwapSwapFails) {
  TestDidNotSwap(cc::SwapPromise::SWAP_FAILS);
}

TEST_F(FrameSwapMessageQueueTest, TestDidNotSwapCommitFails) {
  ScopedVector<IPC::Message> messages;

  QueueNextSwapMessage(CloneMessage(first_message_));
  QueueVisualStateMessage(2, CloneMessage(second_message_));
  QueueVisualStateMessage(3, CloneMessage(third_message_));

  queue_->DidNotSwap(2, cc::SwapPromise::COMMIT_FAILS, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, second_message_.routing_id()));
  messages.clear();

  queue_->DidNotSwap(3, cc::SwapPromise::COMMIT_FAILS, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, third_message_.routing_id()));
  messages.clear();

  DrainMessages(1, &messages);
  ASSERT_EQ(1u, messages.size());
  ASSERT_TRUE(HasMessageForId(messages, first_message_.routing_id()));
}

class NotifiesDeletionMessage : public IPC::Message {
 public:
  NotifiesDeletionMessage(bool* deleted, const IPC::Message& other)
      : IPC::Message(other), deleted_(deleted) {}
  virtual ~NotifiesDeletionMessage() { *deleted_ = true; }

 private:
  bool* deleted_;
};

TEST_F(FrameSwapMessageQueueTest, TestDeletesNextSwapMessage) {
  bool message_deleted = false;
  QueueNextSwapMessage(make_scoped_ptr(new NotifiesDeletionMessage(
                                           &message_deleted, first_message_))
                           .PassAs<IPC::Message>());
  queue_ = NULL;
  ASSERT_TRUE(message_deleted);
}

TEST_F(FrameSwapMessageQueueTest, TestDeletesVisualStateMessage) {
  bool message_deleted = false;
  QueueVisualStateMessage(1,
                          make_scoped_ptr(new NotifiesDeletionMessage(
                                              &message_deleted, first_message_))
                              .PassAs<IPC::Message>());
  queue_ = NULL;
  ASSERT_TRUE(message_deleted);
}

TEST_F(FrameSwapMessageQueueTest, TestDeletesQueuedVisualStateMessage) {
  bool message_deleted = false;
  QueueVisualStateMessage(1,
                          make_scoped_ptr(new NotifiesDeletionMessage(
                                              &message_deleted, first_message_))
                              .PassAs<IPC::Message>());
  queue_->DidSwap(1);
  queue_ = NULL;
  ASSERT_TRUE(message_deleted);
}

}  // namespace content
