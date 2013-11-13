// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_impl.h"

#include <limits>

#include "base/basictypes.h"
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
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h, NULL, NULL, NULL, NULL,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(3u, info.GetReadMessageCallCount());

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

  // |CreateMessagePipe()|:
  {
    MojoHandle h;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(NULL, NULL));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(&h, NULL));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->CreateMessagePipe(NULL, &h));
  }

  // |WriteMessage()|:
  // Only check arguments checked by |CoreImpl|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(MOJO_HANDLE_INVALID, NULL, 0, NULL, 0,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    MojoHandle handles[2] = { MOJO_HANDLE_INVALID, MOJO_HANDLE_INVALID };

    // Null |handles| with nonzero |num_handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, NULL, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    // Checked by |CoreImpl|, shouldn't go through to the dispatcher.
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Huge handle count (implausibly big on some systems -- more than can be
    // stored in a 32-bit address space).
    // Note: This may return either |MOJO_RESULT_INVALID_ARGUMENT| or
    // |MOJO_RESULT_RESOURCE_EXHAUSTED|, depending on whether it's plausible or
    // not.
    EXPECT_NE(MOJO_RESULT_OK,
              core()->WriteMessage(h, NULL, 0, handles,
                                   std::numeric_limits<uint32_t>::max(),
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Huge handle count (plausibly big).
    EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              core()->WriteMessage(h, NULL, 0, handles,
                                   std::numeric_limits<uint32_t>::max() /
                                       sizeof(handles[0]),
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Invalid handle in |handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Two invalid handles in |handles|.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    // Can't send a handle over itself.
    handles[0] = h;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(0u, info.GetWriteMessageCallCount());

    MockHandleInfo info_2;
    MojoHandle h_2 = CreateMockHandle(&info_2);

    // This is "okay", but |MockDispatcher| doesn't implement it.
    handles[0] = h_2;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              core()->WriteMessage(h, NULL, 0, handles, 1,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is still invalid.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // One of the |handles| is the same as |handle|.
    handles[1] = h;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // Can't send a handle twice in the same message.
    handles[1] = h_2;
    EXPECT_EQ(MOJO_RESULT_BUSY,
              core()->WriteMessage(h, NULL, 0, handles, 2,
                                   MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(1u, info.GetWriteMessageCallCount());

    // Note: Since we never successfully sent anything with it, |h_2| should
    // still be valid.
    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_2));

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }

  // |ReadMessage()|:
  // Only check arguments checked by |CoreImpl|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(MOJO_HANDLE_INVALID, NULL, NULL, NULL, NULL,
                                  MOJO_READ_MESSAGE_FLAG_NONE));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);

    uint32_t handle_count = 1;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(h, NULL, NULL, NULL, &handle_count,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
    // Checked by |CoreImpl|, shouldn't go through to the dispatcher.
    EXPECT_EQ(0u, info.GetReadMessageCallCount());

    // Okay.
    handle_count = 0;
    EXPECT_EQ(MOJO_RESULT_OK,
              core()->ReadMessage(h, NULL, NULL, NULL, &handle_count,
                                  MOJO_READ_MESSAGE_FLAG_NONE));
    // Checked by |CoreImpl|, shouldn't go through to the dispatcher.
    EXPECT_EQ(1u, info.GetReadMessageCallCount());

    EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h));
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

TEST_F(CoreImplTest, MessagePipeBasicLocalHandlePassing) {
  const char kHello[] = "hello";
  const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
  const char kWorld[] = "world!!!";
  const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
  char buffer[100];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t num_bytes;
  MojoHandle handles[10];
  uint32_t num_handles;
  MojoHandle h_received;

  MojoHandle h_passing[2];
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(&h_passing[0], &h_passing[1]));

  MojoHandle h_passed[2];
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(&h_passed[0], &h_passed[1]));

  // Make sure that |h_passing[]| work properly.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_WAIT_FLAG_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  // Make sure that |h_passed[]| work properly.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passed[1], MOJO_WAIT_FLAG_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passed[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  // Send |h_passed[1]| from |h_passing[0]| to |h_passing[1]|.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0],
                                 kWorld, kWorldSize,
                                 &h_passed[1], 1,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_passing[1], MOJO_WAIT_FLAG_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1],
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kWorldSize, num_bytes);
  EXPECT_STREQ(kWorld, buffer);
  EXPECT_EQ(1u, num_handles);
  h_received = handles[0];
  EXPECT_NE(h_received, MOJO_HANDLE_INVALID);
  EXPECT_NE(h_received, h_passing[0]);
  EXPECT_NE(h_received, h_passing[1]);
  EXPECT_NE(h_received, h_passed[0]);

  // Note: We rely on the Mojo system not re-using handle values very often.
  EXPECT_NE(h_received, h_passed[1]);

  // |h_passed[1]| should no longer be valid; check that trying to close it
  // fails. See above note.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h_passed[1]));

  // Write to |h_passed[0]|. Should receive on |h_received|.
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passed[0],
                                 kHello, kHelloSize,
                                 NULL, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE));
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->Wait(h_received, MOJO_WAIT_FLAG_READABLE, 1000000000));
  num_bytes = kBufferSize;
  num_handles = arraysize(handles);
  EXPECT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_received,
                                buffer, &num_bytes,
                                handles, &num_handles,
                                MOJO_READ_MESSAGE_FLAG_NONE));
  EXPECT_EQ(kHelloSize, num_bytes);
  EXPECT_STREQ(kHello, buffer);
  EXPECT_EQ(0u, num_handles);

  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_passed[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->Close(h_received));
}

}  // namespace
}  // namespace system
}  // namespace mojo
