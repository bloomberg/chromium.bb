// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/lib/message_queue.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

void AllocRequestMessage(uint32_t name, const char* text, Message* message) {
  size_t payload_size = strlen(text) + 1;  // Plus null terminator.
  internal::RequestMessageBuilder builder(name, payload_size);
  memcpy(builder.buffer()->Allocate(payload_size), text, payload_size);
  builder.Finish(message);
}

void AllocResponseMessage(uint32_t name,
                          const char* text,
                          uint64_t request_id,
                          Message* message) {
  size_t payload_size = strlen(text) + 1;  // Plus null terminator.
  internal::ResponseMessageBuilder builder(name, payload_size, request_id);
  memcpy(builder.buffer()->Allocate(payload_size), text, payload_size);
  builder.Finish(message);
}

class MessageAccumulator : public MessageReceiver {
 public:
  explicit MessageAccumulator(internal::MessageQueue* queue) : queue_(queue) {}

  bool Accept(Message* message) override {
    queue_->Push(message);
    return true;
  }

 private:
  internal::MessageQueue* queue_;
};

class ResponseGenerator : public MessageReceiverWithResponder {
 public:
  ResponseGenerator() {}

  bool Accept(Message* message) override { return false; }

  bool AcceptWithResponder(Message* message,
                           MessageReceiver* responder) override {
    EXPECT_TRUE(message->has_flag(internal::kMessageExpectsResponse));

    return SendResponse(message->name(), message->request_id(), responder);
  }

  bool SendResponse(uint32_t name,
                    uint64_t request_id,
                    MessageReceiver* responder) {
    Message response;
    AllocResponseMessage(name, "world", request_id, &response);

    bool result = responder->Accept(&response);
    delete responder;
    return result;
  }
};

class LazyResponseGenerator : public ResponseGenerator {
 public:
  LazyResponseGenerator() : responder_(nullptr), name_(0), request_id_(0) {}

  ~LazyResponseGenerator() override { delete responder_; }

  bool AcceptWithResponder(Message* message,
                           MessageReceiver* responder) override {
    name_ = message->name();
    request_id_ = message->request_id();
    responder_ = responder;
    return true;
  }

  bool has_responder() const { return !!responder_; }

  void Complete() {
    SendResponse(name_, request_id_, responder_);
    responder_ = nullptr;
  }

 private:
  MessageReceiver* responder_;
  uint32_t name_;
  uint64_t request_id_;
};

class RouterTest : public testing::Test {
 public:
  RouterTest() {}

  virtual void SetUp() override {
    CreateMessagePipe(nullptr, &handle0_, &handle1_);
  }

  virtual void TearDown() override {}

  void PumpMessages() { loop_.RunUntilIdle(); }

 protected:
  ScopedMessagePipeHandle handle0_;
  ScopedMessagePipeHandle handle1_;

 private:
  Environment env_;
  RunLoop loop_;
};

TEST_F(RouterTest, BasicRequestResponse) {
  internal::Router router0(handle0_.Pass(), internal::FilterChain());
  internal::Router router1(handle1_.Pass(), internal::FilterChain());

  ResponseGenerator generator;
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  internal::MessageQueue message_queue;
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  PumpMessages();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("world"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(RouterTest, BasicRequestResponse_Synchronous) {
  internal::Router router0(handle0_.Pass(), internal::FilterChain());
  internal::Router router1(handle1_.Pass(), internal::FilterChain());

  ResponseGenerator generator;
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  internal::MessageQueue message_queue;
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  router1.WaitForIncomingMessage();
  router0.WaitForIncomingMessage();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("world"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(RouterTest, RequestWithNoReceiver) {
  internal::Router router0(handle0_.Pass(), internal::FilterChain());
  internal::Router router1(handle1_.Pass(), internal::FilterChain());

  // Without an incoming receiver set on router1, we expect router0 to observe
  // an error as a result of sending a message.

  Message request;
  AllocRequestMessage(1, "hello", &request);

  internal::MessageQueue message_queue;
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  PumpMessages();

  EXPECT_TRUE(router0.encountered_error());
  EXPECT_TRUE(router1.encountered_error());
  EXPECT_TRUE(message_queue.IsEmpty());
}

TEST_F(RouterTest, LateResponse) {
  // Test that things won't blow up if we try to send a message to a
  // MessageReceiver, which was given to us via AcceptWithResponder,
  // after the router has gone away.

  LazyResponseGenerator generator;
  {
    internal::Router router0(handle0_.Pass(), internal::FilterChain());
    internal::Router router1(handle1_.Pass(), internal::FilterChain());

    router1.set_incoming_receiver(&generator);

    Message request;
    AllocRequestMessage(1, "hello", &request);

    internal::MessageQueue message_queue;
    router0.AcceptWithResponder(&request,
                                new MessageAccumulator(&message_queue));

    PumpMessages();

    EXPECT_TRUE(generator.has_responder());
  }

  generator.Complete();  // This should end up doing nothing.
}

}  // namespace
}  // namespace test
}  // namespace mojo
