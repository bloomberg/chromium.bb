// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core_cpp.h"

#include <map>

#include "mojo/public/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(CoreCppTest, Basic) {
  // Basic |Handle| implementation:
  {
    EXPECT_EQ(MOJO_HANDLE_INVALID, kInvalidHandleValue);

    Handle h_0;
    EXPECT_EQ(kInvalidHandleValue, h_0.value());
    EXPECT_EQ(kInvalidHandleValue, *h_0.mutable_value());
    EXPECT_FALSE(h_0.is_valid());

    Handle h_1(static_cast<MojoHandle>(123));
    EXPECT_EQ(static_cast<MojoHandle>(123), h_1.value());
    EXPECT_EQ(static_cast<MojoHandle>(123), *h_1.mutable_value());
    EXPECT_TRUE(h_1.is_valid());
    *h_1.mutable_value() = static_cast<MojoHandle>(456);
    EXPECT_EQ(static_cast<MojoHandle>(456), h_1.value());
    EXPECT_TRUE(h_1.is_valid());

    h_1.swap(h_0);
    EXPECT_EQ(static_cast<MojoHandle>(456), h_0.value());
    EXPECT_TRUE(h_0.is_valid());
    EXPECT_FALSE(h_1.is_valid());

    h_1.set_value(static_cast<MojoHandle>(789));
    h_0.swap(h_1);
    EXPECT_EQ(static_cast<MojoHandle>(789), h_0.value());
    EXPECT_TRUE(h_0.is_valid());
    EXPECT_EQ(static_cast<MojoHandle>(456), h_1.value());
    EXPECT_TRUE(h_1.is_valid());

    // Make sure copy constructor works.
    Handle h_2(h_0);
    EXPECT_EQ(static_cast<MojoHandle>(789), h_2.value());
    // And assignment.
    h_2 = h_1;
    EXPECT_EQ(static_cast<MojoHandle>(456), h_2.value());

    // Make sure that we can put |Handle|s into |std::map|s.
    h_0 = Handle(static_cast<MojoHandle>(987));
    h_1 = Handle(static_cast<MojoHandle>(654));
    h_2 = Handle(static_cast<MojoHandle>(321));
    Handle h_3;
    std::map<Handle, int> handle_to_int;
    handle_to_int[h_0] = 0;
    handle_to_int[h_1] = 1;
    handle_to_int[h_2] = 2;
    handle_to_int[h_3] = 3;

    EXPECT_EQ(4u, handle_to_int.size());
    EXPECT_FALSE(handle_to_int.find(h_0) == handle_to_int.end());
    EXPECT_EQ(0, handle_to_int[h_0]);
    EXPECT_FALSE(handle_to_int.find(h_1) == handle_to_int.end());
    EXPECT_EQ(1, handle_to_int[h_1]);
    EXPECT_FALSE(handle_to_int.find(h_2) == handle_to_int.end());
    EXPECT_EQ(2, handle_to_int[h_2]);
    EXPECT_FALSE(handle_to_int.find(h_3) == handle_to_int.end());
    EXPECT_EQ(3, handle_to_int[h_3]);
    EXPECT_TRUE(handle_to_int.find(Handle(static_cast<MojoHandle>(13579))) ==
                    handle_to_int.end());

    // TODO(vtl): With C++11, support |std::unordered_map|s, etc. (Or figure out
    // how to support the variations of |hash_map|.)
  }

  // |Handle|/|ScopedHandle| functions:
  {
    ScopedHandle h;

    EXPECT_EQ(kInvalidHandleValue, h.get().value());

    // This should be a no-op.
    Close(h.Pass());

    // It should still be invalid.
    EXPECT_EQ(kInvalidHandleValue, h.get().value());

    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              Wait(h.get(), MOJO_WAIT_FLAG_EVERYTHING, 1000000));

    std::vector<Handle> wh;
    wh.push_back(h.get());
    std::vector<MojoWaitFlags> wf;
    wf.push_back(MOJO_WAIT_FLAG_EVERYTHING);
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              WaitMany(wh, wf, MOJO_DEADLINE_INDEFINITE));
  }

  // |MessagePipeHandle|/|ScopedMessagePipeHandle| functions:
  {
    MessagePipeHandle h_invalid;
    EXPECT_FALSE(h_invalid.is_valid());
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              WriteMessageRaw(h_invalid,
                              NULL, 0,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    char buffer[10] = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              WriteMessageRaw(h_invalid,
                              buffer, sizeof(buffer),
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              ReadMessageRaw(h_invalid,
                             NULL, NULL,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));
    uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              ReadMessageRaw(h_invalid,
                             buffer, &buffer_size,
                             NULL, NULL,
                             MOJO_READ_MESSAGE_FLAG_NONE));

    // Basic tests of waiting and closing.
    MojoHandle hv_0 = kInvalidHandleValue;
    {
      ScopedMessagePipeHandle h_0;
      ScopedMessagePipeHandle h_1;
      EXPECT_FALSE(h_0.get().is_valid());
      EXPECT_FALSE(h_1.get().is_valid());

      CreateMessagePipe(&h_0, &h_1);
      EXPECT_TRUE(h_0.get().is_valid());
      EXPECT_TRUE(h_1.get().is_valid());
      EXPECT_NE(h_0.get().value(), h_1.get().value());
      // Save the handle values, so we can check that things got closed
      // correctly.
      hv_0 = h_0.get().value();
      MojoHandle hv_1 = h_1.get().value();

      EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED,
                Wait(h_0.get(), MOJO_WAIT_FLAG_READABLE, 0));
      std::vector<Handle> wh;
      wh.push_back(h_0.get());
      wh.push_back(h_1.get());
      std::vector<MojoWaitFlags> wf;
      wf.push_back(MOJO_WAIT_FLAG_READABLE);
      wf.push_back(MOJO_WAIT_FLAG_WRITABLE);
      EXPECT_EQ(1, WaitMany(wh, wf, 1000));

      // Test closing |h_1| explicitly.
      Close(h_1.Pass());
      EXPECT_FALSE(h_1.get().is_valid());

      // Make sure |h_1| is closed.
      EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
                MojoWait(hv_1,
                         MOJO_WAIT_FLAG_EVERYTHING,
                         MOJO_DEADLINE_INDEFINITE));

      EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
                Wait(h_0.get(), MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));
    }
    // |hv_0| should have been closed when |h_0| went out of scope, so this
    // close should fail.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(hv_0));

    // Actually test writing/reading messages.
    {
      ScopedMessagePipeHandle h_0;
      ScopedMessagePipeHandle h_1;
      CreateMessagePipe(&h_0, &h_1);

      const char kHello[] = "hello";
      const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(h_0.get(),
                                kHello, kHelloSize,
                                NULL, 0,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));
      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(h_1.get(), MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));
      char buffer[10] = { 0 };
      uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
      EXPECT_EQ(MOJO_RESULT_OK,
                ReadMessageRaw(h_1.get(),
                               buffer, &buffer_size,
                               NULL, NULL,
                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kHelloSize, buffer_size);
      EXPECT_STREQ(kHello, buffer);

      // Send a handle over the previously-establish |MessagePipe|.
      ScopedMessagePipeHandle h_2;
      ScopedMessagePipeHandle h_3;
      CreateMessagePipe(&h_2, &h_3);

      // Write a message to |h_2|, before we send |h_3|.
      const char kWorld[] = "world!";
      const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(h_2.get(),
                                kWorld, kWorldSize,
                                NULL, 0,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));

      // Send |h_3| over |h_1| to |h_0|.
      MojoHandle handles[5];
      handles[0] = h_3.release().value();
      EXPECT_NE(kInvalidHandleValue, handles[0]);
      EXPECT_FALSE(h_3.get().is_valid());
      uint32_t handles_count = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(h_1.get(),
                                kHello, kHelloSize,
                                handles, handles_count,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));
      // |handles[0]| should actually be invalid now.
      EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(handles[0]));

      // Read "hello" and the sent handle.
      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(h_0.get(), MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));
      memset(buffer, 0, sizeof(buffer));
      buffer_size = static_cast<uint32_t>(sizeof(buffer));
      for (size_t i = 0; i < MOJO_ARRAYSIZE(handles); i++)
        handles[i] = kInvalidHandleValue;
      handles_count = static_cast<uint32_t>(MOJO_ARRAYSIZE(handles));
      EXPECT_EQ(MOJO_RESULT_OK,
                ReadMessageRaw(h_0.get(),
                               buffer, &buffer_size,
                               handles, &handles_count,
                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kHelloSize, buffer_size);
      EXPECT_STREQ(kHello, buffer);
      EXPECT_EQ(1u, handles_count);
      EXPECT_NE(kInvalidHandleValue, handles[0]);

      // Read from the sent/received handle.
      h_3.reset(MessagePipeHandle(handles[0]));
      // Save |handles[0]| to check that it gets properly closed.
      hv_0 = handles[0];
      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(h_3.get(), MOJO_WAIT_FLAG_READABLE,
                     MOJO_DEADLINE_INDEFINITE));
      memset(buffer, 0, sizeof(buffer));
      buffer_size = static_cast<uint32_t>(sizeof(buffer));
      for (size_t i = 0; i < MOJO_ARRAYSIZE(handles); i++)
        handles[i] = kInvalidHandleValue;
      handles_count = static_cast<uint32_t>(MOJO_ARRAYSIZE(handles));
      EXPECT_EQ(MOJO_RESULT_OK,
                ReadMessageRaw(h_3.get(),
                               buffer, &buffer_size,
                               handles, &handles_count,
                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kWorldSize, buffer_size);
      EXPECT_STREQ(kWorld, buffer);
      EXPECT_EQ(0u, handles_count);
    }
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(hv_0));
  }

  // TODO(vtl): Test |CloseRaw()|.
  // TODO(vtl): Test |reset()| more thoroughly?
}

TEST(CoreCppTest, TearDownWithMessagesEnqueued) {
  // Tear down a |MessagePipe| which still has a message enqueued, with the
  // message also having a valid |MessagePipe| handle.
  {
    ScopedMessagePipeHandle h_0;
    ScopedMessagePipeHandle h_1;
    CreateMessagePipe(&h_0, &h_1);

    // Send a handle over the previously-establish |MessagePipe|.
    ScopedMessagePipeHandle h_2;
    ScopedMessagePipeHandle h_3;
    CreateMessagePipe(&h_2, &h_3);

    // Write a message to |h_2|, before we send |h_3|.
    const char kWorld[] = "world!";
    const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_2.get(),
                              kWorld, kWorldSize,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // And also a message to |h_3|.
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_3.get(),
                              kWorld, kWorldSize,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Send |h_3| over |h_1| to |h_0|.
    const char kHello[] = "hello";
    const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
    MojoHandle h_3_value;
    h_3_value = h_3.release().value();
    EXPECT_NE(kInvalidHandleValue, h_3_value);
    EXPECT_FALSE(h_3.get().is_valid());
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_1.get(),
                              kHello, kHelloSize,
                              &h_3_value, 1,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // |h_3_value| should actually be invalid now.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(h_3_value));

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_0.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_1.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_2.release().value()));
  }

  // Do this in a different order: make the enqueued |MessagePipe| handle only
  // half-alive.
  {
    ScopedMessagePipeHandle h_0;
    ScopedMessagePipeHandle h_1;
    CreateMessagePipe(&h_0, &h_1);

    // Send a handle over the previously-establish |MessagePipe|.
    ScopedMessagePipeHandle h_2;
    ScopedMessagePipeHandle h_3;
    CreateMessagePipe(&h_2, &h_3);

    // Write a message to |h_2|, before we send |h_3|.
    const char kWorld[] = "world!";
    const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_2.get(),
                              kWorld, kWorldSize,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // And also a message to |h_3|.
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_3.get(),
                              kWorld, kWorldSize,
                              NULL, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Send |h_3| over |h_1| to |h_0|.
    const char kHello[] = "hello";
    const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
    MojoHandle h_3_value;
    h_3_value = h_3.release().value();
    EXPECT_NE(kInvalidHandleValue, h_3_value);
    EXPECT_FALSE(h_3.get().is_valid());
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h_1.get(),
                              kHello, kHelloSize,
                              &h_3_value, 1,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // |h_3_value| should actually be invalid now.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(h_3_value));

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_2.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_0.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h_1.release().value()));
  }
}

TEST(CoreCppTest, GetTimeTicksNow) {
  const MojoTimeTicks start = GetTimeTicksNow();
  EXPECT_NE(static_cast<MojoTimeTicks>(0), start)
      << "TimeTicks should return non-zero value";
}

}  // namespace
}  // namespace mojo
