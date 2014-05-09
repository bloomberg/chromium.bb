// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/scratch_buffer.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

// Tests that basic Array operations work.
TEST(ArrayTest, Basic) {
  Environment env;

  // 8 bytes for the array, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 8*sizeof(char));
  EXPECT_EQ(16u, buf.size());

  Array<char>::Builder builder(8);
  EXPECT_EQ(8u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    char val = static_cast<char>(i*2);
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<char> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    char val = static_cast<char>(i) * 2;
    EXPECT_EQ(val, array[i]);
  }
}

// Tests that basic Array<bool> operations work, and that it's packed into 1
// bit per element.
TEST(ArrayTest, Bool) {
  Environment env;

  // 8 bytes for the array header, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 3);
  EXPECT_EQ(16u, buf.size());

  // An array of 64 bools can fit into 8 bytes.
  Array<bool>::Builder builder(64);
  EXPECT_EQ(64u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    bool val = i % 3 == 0;
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<bool> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    bool val = i % 3 == 0;
    EXPECT_EQ(val, array[i]);
  }
}

// Tests that Array<Handle> supports transferring handles.
TEST(ArrayTest, Handle) {
  Environment env;

  AllocationScope scope;

  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  Array<MessagePipeHandle>::Builder handles_builder(2);
  handles_builder[0] = pipe0.Pass();
  handles_builder[1].reset(pipe1.release());

  EXPECT_FALSE(pipe0.is_valid());
  EXPECT_FALSE(pipe1.is_valid());

  Array<MessagePipeHandle> handles = handles_builder.Finish();
  EXPECT_TRUE(handles[0].is_valid());
  EXPECT_TRUE(handles[1].is_valid());

  pipe0 = handles[0].Pass();
  EXPECT_TRUE(pipe0.is_valid());
  EXPECT_FALSE(handles[0].is_valid());
}

// Tests that Array<Handle> supports closing handles.
TEST(ArrayTest, HandlesAreClosed) {
  Environment env;

  ScopedMessagePipeHandle msg_pipe0, msg_pipe1;
  CreateMessagePipe(&msg_pipe0, &msg_pipe1);

  ScopedHandle pipe0 = ScopedHandle::From(msg_pipe0.Pass());
  ScopedHandle pipe1 = ScopedHandle::From(msg_pipe1.Pass());

  MojoHandle pipe0_value = pipe0.get().value();
  MojoHandle pipe1_value = pipe1.get().value();

  {
    AllocationScope scope;

    Array<Handle>::Builder handles_builder(2);
    handles_builder[0] = pipe0.Pass();
    handles_builder[1].reset(pipe1.release());

    MOJO_ALLOW_UNUSED Array<Handle> handles =
        handles_builder.Finish();
  }

  // We expect the pipes to have been closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe0_value));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe1_value));
}

// Tests that Array<MessagePipeHandle> supports closing handles.
TEST(ArrayTest, MessagePipeHandlesAreClosed) {
  Environment env;

  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  MojoHandle pipe0_value = pipe0.get().value();
  MojoHandle pipe1_value = pipe1.get().value();

  {
    AllocationScope scope;

    Array<MessagePipeHandle>::Builder handles_builder(2);
    handles_builder[0] = pipe0.Pass();
    handles_builder[1].reset(pipe1.release());

    MOJO_ALLOW_UNUSED Array<MessagePipeHandle> handles =
        handles_builder.Finish();
  }

  // We expect the pipes to have been closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe0_value));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe1_value));
}

}  // namespace
}  // namespace test
}  // namespace mojo
