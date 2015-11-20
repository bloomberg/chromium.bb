// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/lib/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_filter.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "mojo/public/cpp/bindings/tests/router_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::InterfaceEndpointClient;
using mojo::internal::MultiplexRouter;
using mojo::internal::ScopedInterfaceEndpointHandle;

class MultiplexRouterTest : public testing::Test {
 public:
  MultiplexRouterTest() : loop_(common::MessagePumpMojo::Create()) {}

  void SetUp() override {
    MessagePipe pipe;
    router0_ = new MultiplexRouter(true, pipe.handle0.Pass());
    router1_ = new MultiplexRouter(true, pipe.handle1.Pass());
    router0_->CreateEndpointHandlePair(&endpoint0_, &endpoint1_);
    endpoint1_ =
        EmulatePassingEndpointHandle(endpoint1_.Pass(), router1_.get());
  }

  void TearDown() override {}

  void PumpMessages() { loop_.RunUntilIdle(); }

  ScopedInterfaceEndpointHandle EmulatePassingEndpointHandle(
      ScopedInterfaceEndpointHandle handle,
      MultiplexRouter* target) {
    CHECK(!handle.is_local());

    return target->CreateLocalEndpointHandle(handle.release());
  }

 protected:
  scoped_refptr<MultiplexRouter> router0_;
  scoped_refptr<MultiplexRouter> router1_;
  ScopedInterfaceEndpointHandle endpoint0_;
  ScopedInterfaceEndpointHandle endpoint1_;

 private:
  base::MessageLoop loop_;
};

TEST_F(MultiplexRouterTest, BasicRequestResponse) {
  InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));
  ResponseGenerator generator;
  InterfaceEndpointClient client1(endpoint1_.Pass(), &generator,
                                  make_scoped_ptr(new PassThroughFilter()));

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  PumpMessages();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  client0.AcceptWithResponder(&request2,
                              new MessageAccumulator(&message_queue));

  PumpMessages();

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(MultiplexRouterTest, BasicRequestResponse_Synchronous) {
  InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));
  ResponseGenerator generator;
  InterfaceEndpointClient client1(endpoint1_.Pass(), &generator,
                                  make_scoped_ptr(new PassThroughFilter()));

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  router1_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  router0_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  client0.AcceptWithResponder(&request2,
                              new MessageAccumulator(&message_queue));

  router1_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  router0_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(MultiplexRouterTest, RequestWithNoReceiver) {
  InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));
  InterfaceEndpointClient client1(endpoint1_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));

  // Without an incoming receiver set on client1, we expect client0 to observe
  // an error as a result of sending a message.

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  PumpMessages();

  EXPECT_TRUE(client0.encountered_error());
  EXPECT_TRUE(client1.encountered_error());
  EXPECT_TRUE(message_queue.IsEmpty());
}

// Tests MultiplexRouter using the LazyResponseGenerator. The responses will not
// be sent until after the requests have been accepted.
TEST_F(MultiplexRouterTest, LazyResponses) {
  InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));
  LazyResponseGenerator generator;
  InterfaceEndpointClient client1(endpoint1_.Pass(), &generator,
                                  make_scoped_ptr(new PassThroughFilter()));

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));
  PumpMessages();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the response.
  EXPECT_TRUE(generator.responder_is_valid());
  generator.CompleteWithResponse();
  PumpMessages();

  // Check the response.
  EXPECT_FALSE(message_queue.IsEmpty());
  Message response;
  message_queue.Pop(&response);
  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  client0.AcceptWithResponder(&request2,
                              new MessageAccumulator(&message_queue));
  PumpMessages();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the second response.
  EXPECT_TRUE(generator.responder_is_valid());
  generator.CompleteWithResponse();
  PumpMessages();

  // Check the second response.
  EXPECT_FALSE(message_queue.IsEmpty());
  message_queue.Pop(&response);
  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

// Tests that if the receiving application destroys the responder_ without
// sending a response, then we trigger connection error at both sides. Moreover,
// both sides still appear to have a valid message pipe handle bound.
TEST_F(MultiplexRouterTest, MissingResponses) {
  InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                  make_scoped_ptr(new PassThroughFilter()));
  bool error_handler_called0 = false;
  client0.set_connection_error_handler(
      [&error_handler_called0]() { error_handler_called0 = true; });

  LazyResponseGenerator generator;
  InterfaceEndpointClient client1(endpoint1_.Pass(), &generator,
                                  make_scoped_ptr(new PassThroughFilter()));
  bool error_handler_called1 = false;
  client1.set_connection_error_handler(
      [&error_handler_called1]() { error_handler_called1 = true; });

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  client0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));
  PumpMessages();

  // The request has been received but no response has been sent.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Destroy the responder MessagerReceiver but don't send any response.
  generator.CompleteWithoutResponse();
  PumpMessages();

  // Check that no response was received.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Connection error handler is called at both sides.
  EXPECT_TRUE(error_handler_called0);
  EXPECT_TRUE(error_handler_called1);

  // The error flag is set at both sides.
  EXPECT_TRUE(client0.encountered_error());
  EXPECT_TRUE(client1.encountered_error());

  // The message pipe handle is valid at both sides.
  EXPECT_TRUE(router0_->is_valid());
  EXPECT_TRUE(router1_->is_valid());
}

TEST_F(MultiplexRouterTest, LateResponse) {
  // Test that things won't blow up if we try to send a message to a
  // MessageReceiver, which was given to us via AcceptWithResponder,
  // after the router has gone away.

  LazyResponseGenerator generator;
  {
    InterfaceEndpointClient client0(endpoint0_.Pass(), nullptr,
                                    make_scoped_ptr(new PassThroughFilter()));
    InterfaceEndpointClient client1(endpoint1_.Pass(), &generator,
                                    make_scoped_ptr(new PassThroughFilter()));

    Message request;
    AllocRequestMessage(1, "hello", &request);

    MessageQueue message_queue;
    client0.AcceptWithResponder(&request,
                                new MessageAccumulator(&message_queue));

    PumpMessages();

    EXPECT_TRUE(generator.has_responder());
  }

  EXPECT_FALSE(generator.responder_is_valid());
  generator.CompleteWithResponse();  // This should end up doing nothing.
}

// TODO(yzshen): add more tests.

}  // namespace
}  // namespace test
}  // namespace mojo
