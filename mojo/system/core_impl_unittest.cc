// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_impl.h"

#include "mojo/system/core_test_base.h"

namespace mojo {
namespace system {
namespace {

class CoreImplTest : public test::CoreTestBase {
};

TEST_F(CoreImplTest, Basic) {
  MockHandleInfo info;

  EXPECT_EQ(0u, info.GetCtorCallCount());
  MojoHandle h = CreateMockHandle(&info);
  EXPECT_EQ(1u, info.GetCtorCallCount());
  EXPECT_NE(h, MOJO_HANDLE_INVALID);

  EXPECT_EQ(0u, info.GetWriteMessageCallCount());
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h, NULL, 0, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, info.GetWriteMessageCallCount());
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(h, NULL, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(2u, info.GetWriteMessageCallCount());

  EXPECT_EQ(0u, info.GetReadMessageCallCount());
  uint32_t num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h, NULL, &num_bytes, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, info.GetReadMessageCallCount());
  num_bytes = 1;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->ReadMessage(h, NULL, &num_bytes, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(2u, info.GetReadMessageCallCount());

  EXPECT_EQ(0u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, MOJO_WAIT_FLAG_EVERYTHING,
                         MOJO_DEADLINE_INDEFINITE));
  EXPECT_EQ(1u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, MOJO_WAIT_FLAG_EVERYTHING, 0));
  EXPECT_EQ(2u, info.GetAddWaiterCallCount());
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h, MOJO_WAIT_FLAG_EVERYTHING, 10 * 1000));
  EXPECT_EQ(3u, info.GetAddWaiterCallCount());
  MojoWaitFlags wait_flags = MOJO_WAIT_FLAG_EVERYTHING;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->WaitMany(&h, &wait_flags, 1, MOJO_DEADLINE_INDEFINITE));
  EXPECT_EQ(4u, info.GetAddWaiterCallCount());

  EXPECT_EQ(0u, info.GetDtorCallCount());
  EXPECT_EQ(0u, info.GetCloseCallCount());
  EXPECT_EQ(0u, info.GetCancelAllWaitersCallCount());
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
  EXPECT_EQ(1u, info.GetCancelAllWaitersCallCount());
  EXPECT_EQ(1u, info.GetCloseCallCount());
  EXPECT_EQ(1u, info.GetDtorCallCount());

  // No waiters should ever have ever been added.
  EXPECT_EQ(0u, info.GetRemoveWaiterCallCount());
}

TEST_F(CoreImplTest, InvalidArguments) {
  // |Close()|:
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(MOJO_HANDLE_INVALID));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(10));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(1000000000));

    // Test a double-close.
    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
    EXPECT_EQ(1u, info.GetCloseCallCount());
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h));
    EXPECT_EQ(1u, info.GetCloseCallCount());
  }

  // |Wait()|:
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(MOJO_HANDLE_INVALID, MOJO_WAIT_FLAG_EVERYTHING,
                           MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->Wait(10, MOJO_WAIT_FLAG_EVERYTHING,
                           MOJO_DEADLINE_INDEFINITE));
  }

  // |WaitMany()|:
  {
    MojoHandle handles[2] = { MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID };
    MojoWaitFlags flags[2] = { MOJO_WAIT_FLAG_EVERYTHING,
                               MOJO_WAIT_FLAG_EVERYTHING };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, flags, 0, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(NULL, flags, 0, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, NULL, 0, MOJO_DEADLINE_INDEFINITE));

    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(NULL, flags, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, NULL, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, flags, 1, MOJO_DEADLINE_INDEFINITE));

    MockHandleInfo info[2];
    handles[0] = CreateMockHandle(&info[0]);

    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              core()->WaitMany(handles, flags, 1, MOJO_DEADLINE_INDEFINITE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, flags, 2, MOJO_DEADLINE_INDEFINITE));
    handles[1] = handles[0] + 1;  // Invalid handle.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WaitMany(handles, flags, 2, MOJO_DEADLINE_INDEFINITE));
    handles[1] = CreateMockHandle(&info[1]);
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              core()->WaitMany(handles, flags, 2, MOJO_DEADLINE_INDEFINITE));

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(handles[0]));
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(handles[1]));
  }
}

// TODO(vtl): test |Wait()| and |WaitMany()| properly
//  - including |WaitMany()| with the same handle more than once (with
//    same/different flags)

TEST_F(CoreImplTest, MessagePipe) {
  MojoHandle h[2];

  EXPECT_EQ(MOJO_RESULT_OK, core()->CreateMessagePipe(&h[0], &h[1]));
  // Should get two distinct, valid handles.
  EXPECT_NE(h[0], MOJO_HANDLE_INVALID);
  EXPECT_NE(h[1], MOJO_HANDLE_INVALID);
  EXPECT_NE(h[0], h[1]);

  // Neither should be readable.
  MojoWaitFlags flags[2] = { MOJO_WAIT_FLAG_READABLE, MOJO_WAIT_FLAG_READABLE };
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->WaitMany(h, flags, 2, 0));

  // Try to read anyway.
  char buffer[1] = { 'a' };
  uint32_t buffer_size = 1;
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            core()->ReadMessage(h[0], buffer, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  // Check that it left its inputs alone.
  EXPECT_EQ('a', buffer[0]);
  EXPECT_EQ(1u, buffer_size);

  // Both should be writable.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[0], MOJO_WAIT_FLAG_WRITABLE, 1000000000));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[1], MOJO_WAIT_FLAG_WRITABLE, 1000000000));

  // Also check that |h[1]| is writable using |WaitMany()|.
  flags[0] = MOJO_WAIT_FLAG_READABLE;
  flags[1] = MOJO_WAIT_FLAG_WRITABLE;
  EXPECT_EQ(1, core()->WaitMany(h, flags, 2, MOJO_DEADLINE_INDEFINITE));

  // Write to |h[1]|.
  buffer[0] = 'b';
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h[1], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Check that |h[0]| is now readable.
  flags[0] = MOJO_WAIT_FLAG_READABLE;
  flags[1] = MOJO_WAIT_FLAG_READABLE;
  EXPECT_EQ(0, core()->WaitMany(h, flags, 2, MOJO_DEADLINE_INDEFINITE));

  // Read from |h[0]|.
  // First, get only the size.
  buffer_size = 0;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            core()->ReadMessage(h[0], NULL, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(1u, buffer_size);
  // Then actually read it.
  buffer[0] = 'c';
  buffer_size = 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h[0], buffer, &buffer_size, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ('b', buffer[0]);
  EXPECT_EQ(1u, buffer_size);

  // |h[0]| should no longer be readable.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
            core()->Wait(h[0], MOJO_WAIT_FLAG_READABLE, 0));

  // Write to |h[0]|.
  buffer[0] = 'd';
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h[0], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Close |h[0]|.
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h[0]));

  // Check that |h[1]| is no longer writable (and will never be).
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_WAIT_FLAG_WRITABLE, 1000000000));

  // Check that |h[1]| is still readable (for the moment).
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h[1], MOJO_WAIT_FLAG_READABLE, 1000000000));

  // Discard a message from |h[1]|.
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            core()->ReadMessage(h[1], NULL, NULL, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_MAY_DISCARD));

  // |h[1]| is no longer readable (and will never be).
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->Wait(h[1], MOJO_WAIT_FLAG_READABLE, 1000000000));

  // Try writing to |h[1]|.
  buffer[0] = 'e';
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->WriteMessage(h[1], buffer, 1, NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h[1]));
}

}  // namespace
}  // namespace system
}  // namespace mojo
