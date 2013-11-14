// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "mojo/public/bindings/lib/bindings_support.h"
#include "mojo/public/bindings/lib/connector.h"
#include "mojo/public/bindings/lib/message_queue.h"
#include "mojo/public/system/macros.h"
#include "mojo/public/tests/simple_bindings_support.h"
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

class BindingsConnectorTest : public testing::Test {
 public:
  BindingsConnectorTest()
      : handle0_(kInvalidHandle),
        handle1_(kInvalidHandle) {
  }

  virtual void SetUp() MOJO_OVERRIDE {
    CreateMessagePipe(&handle0_, &handle1_);
  }

  virtual void TearDown() MOJO_OVERRIDE {
    Close(handle0_);
    Close(handle1_);
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
    bindings_support_.Process();
  }

 protected:
  Handle handle0_;
  Handle handle1_;

 private:
  SimpleBindingsSupport bindings_support_;
};

TEST_F(BindingsConnectorTest, Basic) {
  Connector connector0(handle0_);
  Connector connector1(handle1_);

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  MessageAccumulator accumulator;
  connector1.SetIncomingReceiver(&accumulator);

  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
}

TEST_F(BindingsConnectorTest, Basic_EarlyIncomingReceiver) {
  Connector connector0(handle0_);
  Connector connector1(handle1_);

  MessageAccumulator accumulator;
  connector1.SetIncomingReceiver(&accumulator);

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

TEST_F(BindingsConnectorTest, Basic_TwoMessages) {
  Connector connector0(handle0_);
  Connector connector1(handle1_);

  const char* kText[] = { "hello", "world" };

  for (size_t i = 0; i < MOJO_ARRAYSIZE(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  MessageAccumulator accumulator;
  connector1.SetIncomingReceiver(&accumulator);

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

TEST_F(BindingsConnectorTest, WriteToClosedPipe) {
  Connector connector0(handle0_);

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  Close(handle0_);  // Close the handle before writing to it.

  bool ok = connector0.Accept(&message);
  EXPECT_FALSE(ok);

  EXPECT_TRUE(connector0.EncounteredError());
}

#if 0
// Enable this test once MojoWriteMessage supports passing handles.
TEST_F(BindingsConnectorTest, MessageWithHandles) {
  Connector connector0(handle0_);
  Connector connector1(handle1_);

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  Handle handles[2];
  CreateMessagePipe(&handles[0], &handles[1]);
  message.handles.push_back(handles[0]);
  message.handles.push_back(handles[1]);

  connector0.Accept(&message);

  // The message should have been transferred.
  EXPECT_TRUE(message.data == NULL);
  EXPECT_TRUE(message.handles.empty());

  MessageAccumulator accumulator;
  connector1.SetIncomingReceiver(&accumulator);

  PumpMessages();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(std::string(kText),
            std::string(
                reinterpret_cast<char*>(message_received.data->payload)));
  ASSERT_EQ(2U, message_received.handles.size());
  EXPECT_EQ(handles[0].value, message_received.handles[0].value);
  EXPECT_EQ(handles[1].value, message_received.handles[1].value);
}
#endif

}  // namespace test
}  // namespace mojo
