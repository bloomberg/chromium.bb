// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "mojo/public/bindings/lib/bindings_support.h"
#include "mojo/public/bindings/lib/connector.h"
#include "mojo/public/bindings/lib/message_queue.h"
#include "mojo/public/tests/simple_bindings_support.h"
#include "mojo/public/tests/test_support.h"

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

class BindingsConnectorTest : public TestBase {
 public:
  BindingsConnectorTest()
      : handle0_(kInvalidHandle),
        handle1_(kInvalidHandle) {
  }

  virtual void SetUp() OVERRIDE {
    CreateMessagePipe(&handle0_, &handle1_);
  }

  virtual void TearDown() OVERRIDE {
    Close(handle0_);
    Close(handle1_);
  }

  void AllocMessage(const char* text, Message* message) {
    size_t payload_size = strlen(text) + 1;  // Plus null terminator.
    size_t num_bytes = sizeof(MessageHeader) + payload_size;
    message->data = static_cast<MessageData*>(malloc(num_bytes));
    message->data->header.num_bytes = num_bytes;
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

  for (size_t i = 0; i < arraysize(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  MessageAccumulator accumulator;
  connector1.SetIncomingReceiver(&accumulator);

  PumpMessages();

  for (size_t i = 0; i < arraysize(kText); ++i) {
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

}  // namespace test
}  // namespace mojo
