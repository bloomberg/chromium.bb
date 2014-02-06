// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "mojo/public/bindings/lib/connector.h"
#include "mojo/public/bindings/lib/message_queue.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/system/macros.h"
#include "mojo/public/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

class MessageAccumulator : public MessageReceiver {
 public:
  MessageAccumulator() {
  }

  virtual bool Accept(Message* message) MOJO_OVERRIDE {
    queue_.Push(message);
    return true;
  }

  bool IsEmpty() const {
    return queue_.IsEmpty();
  }

  void Pop(Message* message) {
    queue_.Pop(message);
  }

 private:
  MessageQueue queue_;
};

class ConnectorTest : public testing::Test {
 public:
  ConnectorTest() {
  }

  virtual void SetUp() MOJO_OVERRIDE {
    CreateMessagePipe(&handle0_, &handle1_);
  }

  virtual void TearDown() MOJO_OVERRIDE {
  }

  void AllocMessage(const char* text, Message* message) {
    size_t payload_size = strlen(text) + 1;  // Plus null terminator.
    size_t num_bytes = sizeof(MessageHeader) + payload_size;
    message->data = static_cast<MessageData*>(malloc(num_bytes));
    message->data->header.num_bytes = static_cast<uint32_t>(num_bytes);
    message->data->header.name = 1;
    memcpy(message->data->payload, text, payload_size);
  }

  void PumpMessages() {
    loop_.RunUntilIdle();
  }

 protected:
  ScopedMessagePipeHandle handle0_;
  ScopedMessagePipeHandle handle1_;

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(ConnectorTest, Basic) {
  internal::Connector connector0(handle0_.Pass());
  internal::Connector connector1(handle1_.Pass());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
}

TEST_F(ConnectorTest, Basic_EarlyIncomingReceiver) {
  internal::Connector connector0(handle0_.Pass());
  internal::Connector connector1(handle1_.Pass());

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
}

TEST_F(ConnectorTest, Basic_TwoMessages) {
  internal::Connector connector0(handle0_.Pass());
  internal::Connector connector1(handle1_.Pass());

  const char* kText[] = { "hello", "world" };

  for (size_t i = 0; i < MOJO_ARRAYSIZE(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  PumpMessages();

  for (size_t i = 0; i < MOJO_ARRAYSIZE(kText); ++i) {
    ASSERT_FALSE(accumulator.IsEmpty());

    Message message_received;
    accumulator.Pop(&message_received);

    EXPECT_EQ(std::string(kText[i]),
              std::string(
                  reinterpret_cast<char*>(message_received.data->payload)));
  }
}

TEST_F(ConnectorTest, WriteToClosedPipe) {
  internal::Connector connector0(handle0_.Pass());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  // Close the other end of the pipe.
  handle1_.reset();

  // Not observed yet because we haven't spun the RunLoop yet.
  EXPECT_FALSE(connector0.encountered_error());

  // Write failures are not reported.
  bool ok = connector0.Accept(&message);
  EXPECT_TRUE(ok);

  // Still not observed.
  EXPECT_FALSE(connector0.encountered_error());

  // Spin the RunLoop, and then we should start observing the closed pipe.
  PumpMessages();

  EXPECT_TRUE(connector0.encountered_error());
}

// Enable this test once MojoWriteMessage supports passing handles.
TEST_F(ConnectorTest, MessageWithHandles) {
  internal::Connector connector0(handle0_.Pass());
  internal::Connector connector1(handle1_.Pass());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  ScopedMessagePipeHandle handles[2];
  CreateMessagePipe(&handles[0], &handles[1]);
  message.handles.push_back(handles[0].release());

  connector0.Accept(&message);

  // The message should have been transferred, releasing the handles.
  EXPECT_TRUE(message.handles.empty());

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
  ASSERT_EQ(1U, message_received.handles.size());

  // Now send a message to the transferred handle and confirm it's sent through
  // to the orginal pipe.
  // TODO(vtl): Do we need a better way of "downcasting" the handle types?
  ScopedMessagePipeHandle smph;
  smph.reset(MessagePipeHandle(message_received.handles[0].value()));
  message_received.handles[0] = Handle();  // |smph| now owns this handle.

  internal::Connector connector_received(smph.Pass());
  internal::Connector connector_original(handles[1].Pass());

  AllocMessage(kText, &message);

  connector_received.Accept(&message);
  connector_original.set_incoming_receiver(&accumulator);
  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
}

}  // namespace test
}  // namespace mojo
