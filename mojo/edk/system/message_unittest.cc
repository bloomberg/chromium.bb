// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace edk {
namespace {

using MessageTest = test::MojoTestBase;

// Helper class which provides a base implementation for an unserialized user
// message context and helpers to go between these objects and opaque message
// handles.
class TestMessageBase {
 public:
  virtual ~TestMessageBase() {}

  static MojoMessageHandle MakeMessageHandle(
      std::unique_ptr<TestMessageBase> message) {
    MojoMessageOperationThunks thunks;
    thunks.struct_size = sizeof(MojoMessageOperationThunks);
    thunks.get_serialized_size = &CallGetSerializedSize;
    thunks.serialize_handles = &CallSerializeHandles;
    thunks.serialize_payload = &CallSerializePayload;
    thunks.destroy = &Destroy;
    MojoMessageHandle handle;
    MojoResult rv = MojoCreateMessage(
        reinterpret_cast<uintptr_t>(message.release()), &thunks, &handle);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    return handle;
  }

  template <typename T>
  static std::unique_ptr<T> UnwrapMessageHandle(
      MojoMessageHandle* message_handle) {
    MojoMessageHandle handle = MOJO_HANDLE_INVALID;
    std::swap(handle, *message_handle);
    uintptr_t context;
    MojoResult rv = MojoReleaseMessageContext(handle, &context);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    MojoDestroyMessage(handle);
    return base::WrapUnique(reinterpret_cast<T*>(context));
  }

 protected:
  virtual void GetSerializedSize(size_t* num_bytes, size_t* num_handles) = 0;
  virtual void SerializeHandles(MojoHandle* handles) = 0;
  virtual void SerializePayload(void* buffer) = 0;

 private:
  static void CallGetSerializedSize(uintptr_t context,
                                    size_t* num_bytes,
                                    size_t* num_handles) {
    reinterpret_cast<TestMessageBase*>(context)->GetSerializedSize(num_bytes,
                                                                   num_handles);
  }

  static void CallSerializeHandles(uintptr_t context, MojoHandle* handles) {
    reinterpret_cast<TestMessageBase*>(context)->SerializeHandles(handles);
  }

  static void CallSerializePayload(uintptr_t context, void* buffer) {
    reinterpret_cast<TestMessageBase*>(context)->SerializePayload(buffer);
  }

  static void Destroy(uintptr_t context) {
    delete reinterpret_cast<TestMessageBase*>(context);
  }
};

class NeverSerializedMessage : public TestMessageBase {
 public:
  NeverSerializedMessage(
      const base::Closure& destruction_callback = base::Closure())
      : destruction_callback_(destruction_callback) {}
  ~NeverSerializedMessage() override {
    if (destruction_callback_)
      destruction_callback_.Run();
  }

 private:
  // TestMessageBase:
  void GetSerializedSize(size_t* num_bytes, size_t* num_handles) override {
    NOTREACHED();
  }
  void SerializeHandles(MojoHandle* handles) override { NOTREACHED(); }
  void SerializePayload(void* buffer) override { NOTREACHED(); }

  const base::Closure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(NeverSerializedMessage);
};

class SimpleMessage : public TestMessageBase {
 public:
  SimpleMessage(const std::string& contents,
                const base::Closure& destruction_callback = base::Closure())
      : contents_(contents), destruction_callback_(destruction_callback) {}

  ~SimpleMessage() override {
    if (destruction_callback_)
      destruction_callback_.Run();
  }

  void AddMessagePipe(mojo::ScopedMessagePipeHandle handle) {
    handles_.emplace_back(std::move(handle));
  }

  std::vector<mojo::ScopedMessagePipeHandle>& handles() { return handles_; }

 private:
  // TestMessageBase:
  void GetSerializedSize(size_t* num_bytes, size_t* num_handles) override {
    *num_bytes = contents_.size();
    *num_handles = handles_.size();
  }

  void SerializeHandles(MojoHandle* handles) override {
    ASSERT_TRUE(!handles_.empty());
    for (size_t i = 0; i < handles_.size(); ++i)
      handles[i] = handles_[i].release().value();
    handles_.clear();
  }

  void SerializePayload(void* buffer) override {
    std::copy(contents_.begin(), contents_.end(), static_cast<char*>(buffer));
  }

  const std::string contents_;
  const base::Closure destruction_callback_;
  std::vector<mojo::ScopedMessagePipeHandle> handles_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessage);
};

TEST_F(MessageTest, InvalidMessageObjects) {
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoDestroyMessage(MOJO_MESSAGE_HANDLE_INVALID));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoGetSerializedMessageContents(
                MOJO_MESSAGE_HANDLE_INVALID, nullptr, nullptr, nullptr, nullptr,
                MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSerializeMessage(MOJO_MESSAGE_HANDLE_INVALID));

  MojoMessageHandle message_handle;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateMessage(0, nullptr, &message_handle));
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateMessage(1234, nullptr, nullptr));
}

TEST_F(MessageTest, SendLocalMessageWithContext) {
  // Simple write+read of a message with context. Verifies that such messages
  // are passed through a local pipe without serialization.
  auto message = base::MakeUnique<NeverSerializedMessage>();
  auto* original_message = message.get();

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(
                a, TestMessageBase::MakeMessageHandle(std::move(message)),
                MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle read_message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, &read_message_handle,
                                            MOJO_READ_MESSAGE_FLAG_NONE));
  message = TestMessageBase::UnwrapMessageHandle<NeverSerializedMessage>(
      &read_message_handle);
  EXPECT_EQ(original_message, message.get());

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, DestroyMessageWithContext) {
  // Tests that |MojoDestroyMessage()| destroys any attached context.
  bool was_deleted = false;
  auto message = base::MakeUnique<NeverSerializedMessage>(
      base::Bind([](bool* was_deleted) { *was_deleted = true; }, &was_deleted));
  MojoMessageHandle handle =
      TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_FALSE(was_deleted);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(handle));
  EXPECT_TRUE(was_deleted);
}

const char kTestMessageWithContext1[] = "hello laziness";

#if !defined(OS_IOS)

const char kTestMessageWithContext2[] = "my old friend";
const char kTestMessageWithContext3[] = "something something";
const char kTestMessageWithContext4[] = "do moar ipc";

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageNoHandles, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  auto m = MojoTestBase::ReadMessage(h);
  EXPECT_EQ(kTestMessageWithContext1, m);
}

TEST_F(MessageTest, SerializeSimpleMessageNoHandlesWithContext) {
  RunTestClient("ReceiveMessageNoHandles", [&](MojoHandle h) {
    auto message = base::MakeUnique<SimpleMessage>(kTestMessageWithContext1);
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     MOJO_WRITE_MESSAGE_FLAG_NONE);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageOneHandle, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  MojoHandle h1;
  auto m = MojoTestBase::ReadMessageWithHandles(h, &h1, 1);
  EXPECT_EQ(kTestMessageWithContext1, m);
  MojoTestBase::WriteMessage(h1, kTestMessageWithContext2);
}

TEST_F(MessageTest, SerializeSimpleMessageOneHandleWithContext) {
  RunTestClient("ReceiveMessageOneHandle", [&](MojoHandle h) {
    auto message = base::MakeUnique<SimpleMessage>(kTestMessageWithContext1);
    mojo::MessagePipe pipe;
    message->AddMessagePipe(std::move(pipe.handle0));
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     MOJO_WRITE_MESSAGE_FLAG_NONE);
    EXPECT_EQ(kTestMessageWithContext2,
              MojoTestBase::ReadMessage(pipe.handle1.get().value()));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageWithHandles, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  MojoHandle handles[4];
  auto m = MojoTestBase::ReadMessageWithHandles(h, handles, 4);
  EXPECT_EQ(kTestMessageWithContext1, m);
  MojoTestBase::WriteMessage(handles[0], kTestMessageWithContext1);
  MojoTestBase::WriteMessage(handles[1], kTestMessageWithContext2);
  MojoTestBase::WriteMessage(handles[2], kTestMessageWithContext3);
  MojoTestBase::WriteMessage(handles[3], kTestMessageWithContext4);
}

TEST_F(MessageTest, SerializeSimpleMessageWithHandlesWithContext) {
  RunTestClient("ReceiveMessageWithHandles", [&](MojoHandle h) {
    auto message = base::MakeUnique<SimpleMessage>(kTestMessageWithContext1);
    mojo::MessagePipe pipes[4];
    message->AddMessagePipe(std::move(pipes[0].handle0));
    message->AddMessagePipe(std::move(pipes[1].handle0));
    message->AddMessagePipe(std::move(pipes[2].handle0));
    message->AddMessagePipe(std::move(pipes[3].handle0));
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     MOJO_WRITE_MESSAGE_FLAG_NONE);
    EXPECT_EQ(kTestMessageWithContext1,
              MojoTestBase::ReadMessage(pipes[0].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext2,
              MojoTestBase::ReadMessage(pipes[1].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext3,
              MojoTestBase::ReadMessage(pipes[2].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext4,
              MojoTestBase::ReadMessage(pipes[3].handle1.get().value()));
  });
}

#endif  // !defined(OS_IOS)

TEST_F(MessageTest, SendLocalSimpleMessageWithHandlesWithContext) {
  auto message = base::MakeUnique<SimpleMessage>(kTestMessageWithContext1);
  auto* original_message = message.get();
  mojo::MessagePipe pipes[4];
  MojoHandle original_handles[4] = {
      pipes[0].handle0.get().value(), pipes[1].handle0.get().value(),
      pipes[2].handle0.get().value(), pipes[3].handle0.get().value(),
  };
  message->AddMessagePipe(std::move(pipes[0].handle0));
  message->AddMessagePipe(std::move(pipes[1].handle0));
  message->AddMessagePipe(std::move(pipes[2].handle0));
  message->AddMessagePipe(std::move(pipes[3].handle0));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(
                a, TestMessageBase::MakeMessageHandle(std::move(message)),
                MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle read_message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, &read_message_handle,
                                            MOJO_READ_MESSAGE_FLAG_NONE));
  message =
      TestMessageBase::UnwrapMessageHandle<SimpleMessage>(&read_message_handle);
  EXPECT_EQ(original_message, message.get());
  ASSERT_EQ(4u, message->handles().size());
  EXPECT_EQ(original_handles[0], message->handles()[0].get().value());
  EXPECT_EQ(original_handles[1], message->handles()[1].get().value());
  EXPECT_EQ(original_handles[2], message->handles()[2].get().value());
  EXPECT_EQ(original_handles[3], message->handles()[3].get().value());

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, DropUnreadLocalMessageWithContext) {
  // Verifies that if a message is sent with context over a pipe and the
  // receiver closes without reading the message, the context is properly
  // cleaned up.
  bool message_was_destroyed = false;
  auto message = base::MakeUnique<SimpleMessage>(
      kTestMessageWithContext1,
      base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                 &message_was_destroyed));

  mojo::MessagePipe pipe;
  message->AddMessagePipe(std::move(pipe.handle0));
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(
                a, TestMessageBase::MakeMessageHandle(std::move(message)),
                MOJO_WRITE_MESSAGE_FLAG_NONE));
  MojoClose(a);
  MojoClose(b);

  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(pipe.handle1.get().value(),
                                           MOJO_HANDLE_SIGNAL_PEER_CLOSED));
}

TEST_F(MessageTest, ReadMessageWithContextAsSerializedMessage) {
  bool message_was_destroyed = false;
  std::unique_ptr<TestMessageBase> message =
      base::MakeUnique<NeverSerializedMessage>(
          base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(
                a, TestMessageBase::MakeMessageHandle(std::move(message)),
                MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle message_handle;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(b, &message_handle, MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_FALSE(message_was_destroyed);

  // Not a serialized message, so we can't get serialized contents.
  uint32_t num_bytes = 0;
  void* buffer;
  uint32_t num_handles = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoGetSerializedMessageContents(
                message_handle, &buffer, &num_bytes, nullptr, &num_handles,
                MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE));
  EXPECT_FALSE(message_was_destroyed);

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, ReadSerializedMessageAsMessageWithContext) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  MojoTestBase::WriteMessage(a, "hello there");
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle message_handle;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(b, &message_handle, MOJO_READ_MESSAGE_FLAG_NONE));
  uintptr_t context;
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            MojoReleaseMessageContext(message_handle, &context));
  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, ForceSerializeMessageWithContext) {
  // Basic test - we can serialize a simple message.
  bool message_was_destroyed = false;
  auto message = base::MakeUnique<SimpleMessage>(
      kTestMessageWithContext1,
      base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                 &message_was_destroyed));
  auto message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));

  // Serialize a message with a single handle. Freeing the message should close
  // the handle.
  message_was_destroyed = false;
  message = base::MakeUnique<SimpleMessage>(
      kTestMessageWithContext1,
      base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                 &message_was_destroyed));
  MessagePipe pipe1;
  message->AddMessagePipe(std::move(pipe1.handle0));
  message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(pipe1.handle1.get().value(),
                                           MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  // Serialize a message with a handle and extract its serialized contents.
  message_was_destroyed = false;
  message = base::MakeUnique<SimpleMessage>(
      kTestMessageWithContext1,
      base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                 &message_was_destroyed));
  MessagePipe pipe2;
  message->AddMessagePipe(std::move(pipe2.handle0));
  message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);
  uint32_t num_bytes = 0;
  void* buffer = nullptr;
  uint32_t num_handles = 0;
  MojoHandle extracted_handle;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            MojoGetSerializedMessageContents(
                message_handle, &buffer, &num_bytes, nullptr, &num_handles,
                MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetSerializedMessageContents(
                message_handle, &buffer, &num_bytes, &extracted_handle,
                &num_handles, MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE));
  EXPECT_EQ(std::string(kTestMessageWithContext1).size(), num_bytes);
  EXPECT_EQ(std::string(kTestMessageWithContext1),
            base::StringPiece(static_cast<char*>(buffer), num_bytes));

  // Confirm that the handle we extracted from the serialized message is still
  // connected to the same peer, despite the fact that its handle value may have
  // changed.
  const char kTestMessage[] = "hey you";
  MojoTestBase::WriteMessage(pipe2.handle1.get().value(), kTestMessage);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(extracted_handle, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage, MojoTestBase::ReadMessage(extracted_handle));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, DoubleSerialize) {
  bool message_was_destroyed = false;
  auto message = base::MakeUnique<SimpleMessage>(
      kTestMessageWithContext1,
      base::Bind([](bool* was_destroyed) { *was_destroyed = true; },
                 &message_was_destroyed));
  auto message_handle = TestMessageBase::MakeMessageHandle(std::move(message));

  // Ensure we can safely call |MojoSerializeMessage()| twice on the same
  // message handle.
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoSerializeMessage(message_handle));

  // And also check that we can call it again after we've written and read the
  // message object from a pipe.
  MessagePipe pipe;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(pipe.handle0->value(), message_handle,
                             MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe.handle1->value(), MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(pipe.handle1->value(), &message_handle,
                            MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoSerializeMessage(message_handle));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

}  // namespace
}  // namespace edk
}  // namespace mojo
