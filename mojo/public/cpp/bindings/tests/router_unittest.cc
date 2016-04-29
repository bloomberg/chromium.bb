// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/router.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "mojo/public/cpp/bindings/tests/router_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class RouterTest : public testing::Test {
 public:
  RouterTest() {}

  void SetUp() override {
    CreateMessagePipe(nullptr, &handle0_, &handle1_);
  }

  void TearDown() override {}

  void PumpMessages() { loop_.RunUntilIdle(); }

 protected:
  ScopedMessagePipeHandle handle0_;
  ScopedMessagePipeHandle handle1_;

 private:
  base::MessageLoop loop_;
};

TEST_F(RouterTest, BasicRequestResponse) {
  internal::Router router0(std::move(handle0_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  internal::Router router1(std::move(handle1_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());

  ResponseGenerator generator;
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop;
  router0.AcceptWithResponder(
      &request, new MessageAccumulator(&message_queue, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  base::RunLoop run_loop2;
  router0.AcceptWithResponder(
      &request2,
      new MessageAccumulator(&message_queue, run_loop2.QuitClosure()));

  run_loop2.Run();

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(RouterTest, BasicRequestResponse_Synchronous) {
  internal::Router router0(std::move(handle0_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  internal::Router router1(std::move(handle1_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());

  ResponseGenerator generator;
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  router1.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  router0.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  EXPECT_FALSE(message_queue.IsEmpty());

  Message response;
  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  router0.AcceptWithResponder(&request2,
                              new MessageAccumulator(&message_queue));

  router1.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  router0.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  EXPECT_FALSE(message_queue.IsEmpty());

  message_queue.Pop(&response);

  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

TEST_F(RouterTest, RequestWithNoReceiver) {
  internal::Router router0(std::move(handle0_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  internal::Router router1(std::move(handle1_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());

  // Without an incoming receiver set on router1, we expect router0 to observe
  // an error as a result of sending a message.

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop, run_loop2;
  router0.set_connection_error_handler(run_loop.QuitClosure());
  router1.set_connection_error_handler(run_loop2.QuitClosure());
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));

  run_loop.Run();
  run_loop2.Run();

  EXPECT_TRUE(router0.encountered_error());
  EXPECT_TRUE(router1.encountered_error());
  EXPECT_TRUE(message_queue.IsEmpty());
}

// Tests Router using the LazyResponseGenerator. The responses will not be
// sent until after the requests have been accepted.
TEST_F(RouterTest, LazyResponses) {
  internal::Router router0(std::move(handle0_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  internal::Router router1(std::move(handle1_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());

  base::RunLoop run_loop;
  LazyResponseGenerator generator(run_loop.QuitClosure());
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  base::RunLoop run_loop2;
  router0.AcceptWithResponder(
      &request,
      new MessageAccumulator(&message_queue, run_loop2.QuitClosure()));
  run_loop.Run();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the response.
  EXPECT_TRUE(generator.responder_is_valid());
  generator.CompleteWithResponse();
  run_loop2.Run();

  // Check the response.
  EXPECT_FALSE(message_queue.IsEmpty());
  Message response;
  message_queue.Pop(&response);
  EXPECT_EQ(std::string("hello world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));

  // Send a second message on the pipe.
  base::RunLoop run_loop3;
  LazyResponseGenerator generator2(run_loop3.QuitClosure());

  router1.set_incoming_receiver(&generator2);
  Message request2;
  AllocRequestMessage(1, "hello again", &request2);

  base::RunLoop run_loop4;
  router0.AcceptWithResponder(
      &request2,
      new MessageAccumulator(&message_queue, run_loop4.QuitClosure()));
  run_loop3.Run();

  // The request has been received but the response has not been sent yet.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Send the second response.
  EXPECT_TRUE(generator2.responder_is_valid());
  generator2.CompleteWithResponse();
  run_loop4.Run();

  // Check the second response.
  EXPECT_FALSE(message_queue.IsEmpty());
  message_queue.Pop(&response);
  EXPECT_EQ(std::string("hello again world!"),
            std::string(reinterpret_cast<const char*>(response.payload())));
}

// Tests that if the receiving application destroys the responder_ without
// sending a response, then we trigger connection error at both sides. Moreover,
// both sides still appear to have a valid message pipe handle bound.
TEST_F(RouterTest, MissingResponses) {
  base::RunLoop run_loop0, run_loop1;
  internal::Router router0(std::move(handle0_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  bool error_handler_called0 = false;
  router0.set_connection_error_handler(
      [&error_handler_called0, &run_loop0]() {
        error_handler_called0 = true;
        run_loop0.Quit();
      });

  internal::Router router1(std::move(handle1_), internal::FilterChain(), false,
                           base::ThreadTaskRunnerHandle::Get());
  bool error_handler_called1 = false;
  router1.set_connection_error_handler(
      [&error_handler_called1, &run_loop1]() {
        error_handler_called1 = true;
        run_loop1.Quit();
      });

  base::RunLoop run_loop3;
  LazyResponseGenerator generator(run_loop3.QuitClosure());
  router1.set_incoming_receiver(&generator);
  router1.set_incoming_receiver(&generator);

  Message request;
  AllocRequestMessage(1, "hello", &request);

  MessageQueue message_queue;
  router0.AcceptWithResponder(&request, new MessageAccumulator(&message_queue));
  run_loop3.Run();

  // The request has been received but no response has been sent.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Destroy the responder MessagerReceiver but don't send any response.
  generator.CompleteWithoutResponse();
  run_loop0.Run();
  run_loop1.Run();

  // Check that no response was received.
  EXPECT_TRUE(message_queue.IsEmpty());

  // Connection error handler is called at both sides.
  EXPECT_TRUE(error_handler_called0);
  EXPECT_TRUE(error_handler_called1);

  // The error flag is set at both sides.
  EXPECT_TRUE(router0.encountered_error());
  EXPECT_TRUE(router1.encountered_error());

  // The message pipe handle is valid at both sides.
  EXPECT_TRUE(router0.is_valid());
  EXPECT_TRUE(router1.is_valid());
}

TEST_F(RouterTest, LateResponse) {
  // Test that things won't blow up if we try to send a message to a
  // MessageReceiver, which was given to us via AcceptWithResponder,
  // after the router has gone away.

  base::RunLoop run_loop;
  LazyResponseGenerator generator(run_loop.QuitClosure());
  {
    internal::Router router0(std::move(handle0_), internal::FilterChain(),
                             false, base::ThreadTaskRunnerHandle::Get());
    internal::Router router1(std::move(handle1_), internal::FilterChain(),
                             false, base::ThreadTaskRunnerHandle::Get());

    router1.set_incoming_receiver(&generator);

    Message request;
    AllocRequestMessage(1, "hello", &request);

    MessageQueue message_queue;
    router0.AcceptWithResponder(&request,
                                new MessageAccumulator(&message_queue));

    run_loop.Run();

    EXPECT_TRUE(generator.has_responder());
  }

  EXPECT_FALSE(generator.responder_is_valid());
  generator.CompleteWithResponse();  // This should end up doing nothing.
}

}  // namespace
}  // namespace test
}  // namespace mojo
