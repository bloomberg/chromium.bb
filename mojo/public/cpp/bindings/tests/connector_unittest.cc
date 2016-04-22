// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/connector.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/tests/message_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class MessageAccumulator : public MessageReceiver {
 public:
  MessageAccumulator() {}
  explicit MessageAccumulator(const Closure& closure) : closure_(closure) {}

  bool Accept(Message* message) override {
    queue_.Push(message);
    if (!closure_.is_null()) {
      Closure closure = closure_;
      closure_.reset();
      closure.Run();
    }
    return true;
  }

  bool IsEmpty() const { return queue_.IsEmpty(); }

  void Pop(Message* message) { queue_.Pop(message); }

  void set_closure(const Closure& closure) { closure_ = closure; }

  size_t size() const { return queue_.size(); }

 private:
  MessageQueue queue_;
  Closure closure_;
};

class ConnectorDeletingMessageAccumulator : public MessageAccumulator {
 public:
  ConnectorDeletingMessageAccumulator(internal::Connector** connector)
      : connector_(connector) {}

  bool Accept(Message* message) override {
    delete *connector_;
    *connector_ = nullptr;
    return MessageAccumulator::Accept(message);
  }

 private:
  internal::Connector** connector_;
};

class ReentrantMessageAccumulator : public MessageAccumulator {
 public:
  ReentrantMessageAccumulator(internal::Connector* connector)
      : connector_(connector), number_of_calls_(0) {}

  bool Accept(Message* message) override {
    if (!MessageAccumulator::Accept(message))
      return false;
    number_of_calls_++;
    if (number_of_calls_ == 1) {
      return connector_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
    }
    return true;
  }

  int number_of_calls() { return number_of_calls_; }

 private:
  internal::Connector* connector_;
  int number_of_calls_;
};

class ConnectorTest : public testing::Test {
 public:
  ConnectorTest() : loop_(common::MessagePumpMojo::Create()) {}

  void SetUp() override {
    CreateMessagePipe(nullptr, &handle0_, &handle1_);
  }

  void TearDown() override {}

  void AllocMessage(const char* text, Message* message) {
    size_t payload_size = strlen(text) + 1;  // Plus null terminator.
    internal::MessageBuilder builder(1, payload_size);
    memcpy(builder.buffer()->Allocate(payload_size), text, payload_size);

    builder.message()->MoveTo(message);
  }

 protected:
  ScopedMessagePipeHandle handle0_;
  ScopedMessagePipeHandle handle1_;

 private:
  base::MessageLoop loop_;
};

TEST_F(ConnectorTest, Basic) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  base::RunLoop run_loop;
  MessageAccumulator accumulator(run_loop.QuitClosure());
  connector1.set_incoming_receiver(&accumulator);

  run_loop.Run();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
}

TEST_F(ConnectorTest, Basic_Synchronous) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  connector1.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
}

TEST_F(ConnectorTest, Basic_EarlyIncomingReceiver) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  base::RunLoop run_loop;
  MessageAccumulator accumulator(run_loop.QuitClosure());
  connector1.set_incoming_receiver(&accumulator);

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  run_loop.Run();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
}

TEST_F(ConnectorTest, Basic_TwoMessages) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char* kText[] = {"hello", "world"};

  for (size_t i = 0; i < arraysize(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  for (size_t i = 0; i < arraysize(kText); ++i) {
    if (accumulator.IsEmpty()) {
      base::RunLoop run_loop;
      accumulator.set_closure(run_loop.QuitClosure());
      run_loop.Run();
    }
    ASSERT_FALSE(accumulator.IsEmpty());

    Message message_received;
    accumulator.Pop(&message_received);

    EXPECT_EQ(
        std::string(kText[i]),
        std::string(reinterpret_cast<const char*>(message_received.payload())));
  }
}

TEST_F(ConnectorTest, Basic_TwoMessages_Synchronous) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char* kText[] = {"hello", "world"};

  for (size_t i = 0; i < arraysize(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  MessageAccumulator accumulator;
  connector1.set_incoming_receiver(&accumulator);

  connector1.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText[0]),
      std::string(reinterpret_cast<const char*>(message_received.payload())));

  ASSERT_TRUE(accumulator.IsEmpty());
}

TEST_F(ConnectorTest, WriteToClosedPipe) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  // Close the other end of the pipe.
  handle1_.reset();

  // Not observed yet because we haven't spun the message loop yet.
  EXPECT_FALSE(connector0.encountered_error());

  // Write failures are not reported.
  bool ok = connector0.Accept(&message);
  EXPECT_TRUE(ok);

  // Still not observed.
  EXPECT_FALSE(connector0.encountered_error());

  // Spin the message loop, and then we should start observing the closed pipe.
  base::RunLoop run_loop;
  connector0.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(connector0.encountered_error());
}

TEST_F(ConnectorTest, MessageWithHandles) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message1;
  AllocMessage(kText, &message1);

  MessagePipe pipe;
  message1.mutable_handles()->push_back(pipe.handle0.release());

  connector0.Accept(&message1);

  // The message should have been transferred, releasing the handles.
  EXPECT_TRUE(message1.handles()->empty());

  base::RunLoop run_loop;
  MessageAccumulator accumulator(run_loop.QuitClosure());
  connector1.set_incoming_receiver(&accumulator);

  run_loop.Run();

  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
  ASSERT_EQ(1U, message_received.handles()->size());

  // Now send a message to the transferred handle and confirm it's sent through
  // to the orginal pipe.
  // TODO(vtl): Do we need a better way of "downcasting" the handle types?
  ScopedMessagePipeHandle smph;
  smph.reset(MessagePipeHandle(message_received.handles()->front().value()));
  message_received.mutable_handles()->front() = Handle();
  // |smph| now owns this handle.

  internal::Connector connector_received(
      std::move(smph), internal::Connector::SINGLE_THREADED_SEND,
      base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector_original(
      std::move(pipe.handle1), internal::Connector::SINGLE_THREADED_SEND,
      base::ThreadTaskRunnerHandle::Get());

  Message message2;
  AllocMessage(kText, &message2);

  connector_received.Accept(&message2);
  base::RunLoop run_loop2;
  MessageAccumulator accumulator2(run_loop2.QuitClosure());
  connector_original.set_incoming_receiver(&accumulator2);
  run_loop2.Run();

  ASSERT_FALSE(accumulator2.IsEmpty());

  accumulator2.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
}

TEST_F(ConnectorTest, WaitForIncomingMessageWithError) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  // Close the other end of the pipe.
  handle1_.reset();
  ASSERT_FALSE(connector0.WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE));
}

TEST_F(ConnectorTest, WaitForIncomingMessageWithDeletion) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector* connector1 = new internal::Connector(
      std::move(handle1_), internal::Connector::SINGLE_THREADED_SEND,
      base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);

  ConnectorDeletingMessageAccumulator accumulator(&connector1);
  connector1->set_incoming_receiver(&accumulator);

  connector1->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);

  ASSERT_FALSE(connector1);
  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));
}

TEST_F(ConnectorTest, WaitForIncomingMessageWithReentrancy) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char* kText[] = {"hello", "world"};

  for (size_t i = 0; i < arraysize(kText); ++i) {
    Message message;
    AllocMessage(kText[i], &message);

    connector0.Accept(&message);
  }

  ReentrantMessageAccumulator accumulator(&connector1);
  connector1.set_incoming_receiver(&accumulator);

  for (size_t i = 0; i < arraysize(kText); ++i) {
    if (accumulator.IsEmpty()) {
      base::RunLoop run_loop;
      accumulator.set_closure(run_loop.QuitClosure());
      run_loop.Run();
    }
    ASSERT_FALSE(accumulator.IsEmpty());

    Message message_received;
    accumulator.Pop(&message_received);

    EXPECT_EQ(
        std::string(kText[i]),
        std::string(reinterpret_cast<const char*>(message_received.payload())));
  }

  ASSERT_EQ(2, accumulator.number_of_calls());
}

TEST_F(ConnectorTest, RaiseError) {
  base::RunLoop run_loop, run_loop2;
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  bool error_handler_called0 = false;
  connector0.set_connection_error_handler(
      [&error_handler_called0, &run_loop]() {
        error_handler_called0 = true;
        run_loop.Quit();
      });

  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  bool error_handler_called1 = false;
  connector1.set_connection_error_handler(
      [&error_handler_called1, &run_loop2]() {
        error_handler_called1 = true;
        run_loop2.Quit();
      });

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  connector0.Accept(&message);
  connector0.RaiseError();

  base::RunLoop run_loop3;
  MessageAccumulator accumulator(run_loop3.QuitClosure());
  connector1.set_incoming_receiver(&accumulator);

  run_loop3.Run();

  // Messages sent prior to RaiseError() still arrive at the other end.
  ASSERT_FALSE(accumulator.IsEmpty());

  Message message_received;
  accumulator.Pop(&message_received);

  EXPECT_EQ(
      std::string(kText),
      std::string(reinterpret_cast<const char*>(message_received.payload())));

  run_loop.Run();
  run_loop2.Run();

  // Connection error handler is called at both sides.
  EXPECT_TRUE(error_handler_called0);
  EXPECT_TRUE(error_handler_called1);

  // The error flag is set at both sides.
  EXPECT_TRUE(connector0.encountered_error());
  EXPECT_TRUE(connector1.encountered_error());

  // The message pipe handle is valid at both sides.
  EXPECT_TRUE(connector0.is_valid());
  EXPECT_TRUE(connector1.is_valid());
}

TEST_F(ConnectorTest, PauseWithQueuedMessages) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  // Queue up two messages.
  connector0.Accept(&message);
  connector0.Accept(&message);

  base::RunLoop run_loop;
  // Configure the accumulator such that it pauses after the first message is
  // received.
  MessageAccumulator accumulator([&connector1, &run_loop]() {
    connector1.PauseIncomingMethodCallProcessing();
    run_loop.Quit();
  });
  connector1.set_incoming_receiver(&accumulator);

  run_loop.Run();

  // As we paused after the first message we should only have gotten one
  // message.
  ASSERT_EQ(1u, accumulator.size());
}

TEST_F(ConnectorTest, ProcessWhenNested) {
  internal::Connector connector0(std::move(handle0_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());
  internal::Connector connector1(std::move(handle1_),
                                 internal::Connector::SINGLE_THREADED_SEND,
                                 base::ThreadTaskRunnerHandle::Get());

  const char kText[] = "hello world";

  Message message;
  AllocMessage(kText, &message);

  // Queue up two messages.
  connector0.Accept(&message);
  connector0.Accept(&message);

  base::RunLoop run_loop;
  MessageAccumulator accumulator;
  // When the accumulator gets the first message it spins a nested message
  // loop. The loop is quit when another message is received.
  accumulator.set_closure([&accumulator, &connector1, &run_loop]() {
    base::RunLoop nested_run_loop;
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    accumulator.set_closure([&nested_run_loop]() { nested_run_loop.Quit(); });
    nested_run_loop.Run();
    run_loop.Quit();
  });
  connector1.set_incoming_receiver(&accumulator);

  run_loop.Run();

  ASSERT_EQ(2u, accumulator.size());
}

}  // namespace
}  // namespace test
}  // namespace mojo
