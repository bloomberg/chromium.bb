// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_data_pipe.h"

#include <string.h>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// Validate options.
TEST(LocalDataPipeTest, Creation) {
  // Create using default options.
  {
    // Get default options.
    MojoCreateDataPipeOptions default_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(NULL, &default_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(default_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }

  // Create using non-default options.
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1,  // |element_num_bytes|.
      1000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      4,  // |element_num_bytes|.
      4000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
      7,  // |element_num_bytes|.
      7000000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  // Default capacity.
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
      100,  // |element_num_bytes|.
      0  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeTest, SimpleReadWrite) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1000 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  int32_t elements[10] = { 0 };
  uint32_t num_bytes = 0;

  // Try reading; nothing there yet.
  num_bytes = static_cast<uint32_t>(arraysize(elements) * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            dp->ConsumerReadData(elements, &num_bytes, false));

  // Query; nothing there yet.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Discard; nothing there yet.
  num_bytes = static_cast<uint32_t>(5u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            dp->ConsumerDiscardData(&num_bytes, false));

  // Read with invalid |num_bytes|.
  num_bytes = sizeof(elements[0]) + 1;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerReadData(elements, &num_bytes, false));

  // Write two elements.
  elements[0] = 123;
  elements[1] = 456;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(elements, &num_bytes, false));
  // It should have written everything (even without "all or none").
  EXPECT_EQ(2u * sizeof(elements[0]), num_bytes);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(2 * sizeof(elements[0]), num_bytes);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, false));
  EXPECT_EQ(1u * sizeof(elements[0]), num_bytes);
  EXPECT_EQ(123, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1 * sizeof(elements[0]), num_bytes);

  // Try to read two elements, with "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(elements, &num_bytes, true));
  EXPECT_EQ(-1, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Try to read two elements, without "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, false));
  EXPECT_EQ(456, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  dp->ProducerClose();
  dp->ConsumerClose();
}

TEST(LocalDataPipeTest, BasicProducerWaiting) {
  // Note: We take advantage of the fact that for |LocalDataPipe|, capacities
  // are strict maximums. This is not guaranteed by the API.

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    2 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));

  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    Waiter waiter;

    // Never readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 12));

    // Already writable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 34));

    // Write two elements.
    int32_t elements[2] = { 123, 456 };
    uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);

    // Adding a waiter should now succeed.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 56));
    // And it shouldn't be writable yet.
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0));
    dp->ProducerRemoveWaiter(&waiter);

    // Do it again.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 78));

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(123, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Waiting should now succeed.
    EXPECT_EQ(78, waiter.Wait(1000));
    dp->ProducerRemoveWaiter(&waiter);

    // Try writing, using a two-phase write.
    void* buffer = NULL;
    num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&buffer, &num_bytes, false));
    EXPECT_TRUE(buffer != NULL);
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    static_cast<int32_t*>(buffer)[0] = 789;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerEndWriteData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Add a waiter.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 90));

    // Read one element, using a two-phase read.
    const void* read_buffer = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer, &num_bytes, false));
    EXPECT_TRUE(read_buffer != NULL);
    // Since we only read one element (after having written three in all), the
    // two-phase read should only allow us to read one. This checks an
    // implementation detail!
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(456, static_cast<const int32_t*>(read_buffer)[0]);
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerEndReadData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Waiting should succeed.
    EXPECT_EQ(90, waiter.Wait(1000));
    dp->ProducerRemoveWaiter(&waiter);

    // Write one element.
    elements[0] = 123;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, false));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

    // Add a waiter.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 12));

    // Close the consumer.
    dp->ConsumerClose();

    // It should now be never-writable.
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000));
    dp->ProducerRemoveWaiter(&waiter);

    dp->ProducerClose();
  }
}

TEST(LocalDataPipeTest, BasicConsumerWaiting) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1000 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));

  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    Waiter waiter;

    // Never writable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_WRITABLE, 12));

    // Not yet readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 34));
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0));
    dp->ConsumerRemoveWaiter(&waiter);

    // Write two elements.
    int32_t elements[2] = { 123, 456 };
    uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, true));

    // Should already be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 56));

    // Discard one element.
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerDiscardData(&num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 78));

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(456, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Adding a waiter should now succeed.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 90));

    // Write one element.
    elements[0] = 789;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, true));

    // Waiting should now succeed.
    EXPECT_EQ(90, waiter.Wait(1000));
    dp->ConsumerRemoveWaiter(&waiter);

    // Close the producer.
    dp->ProducerClose();

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 12));

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(789, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Should be never-readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 34));

    dp->ConsumerClose();
  }

  // Test with two-phase APIs and closing the producer with an active consumer
  // waiter.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    Waiter waiter;

    // Write two elements.
    int32_t* elements = NULL;
    void* buffer = NULL;
    // Request room for three (but we'll only write two).
    uint32_t num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&buffer, &num_bytes, true));
    EXPECT_TRUE(buffer != NULL);
    EXPECT_GE(num_bytes, static_cast<uint32_t>(3u * sizeof(elements[0])));
    elements = static_cast<int32_t*>(buffer);
    elements[0] = 123;
    elements[1] = 456;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerEndWriteData(
                  static_cast<uint32_t>(2u * sizeof(elements[0]))));

    // Should already be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 12));

    // Read one element.
    // Request two in all-or-none mode, but only read one.
    const void* read_buffer = NULL;
    num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer, &num_bytes, true));
    EXPECT_TRUE(read_buffer != NULL);
    EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);
    const int32_t* read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(123, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerEndReadData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 34));

    // Read one element.
    // Request three, but not in all-or-none mode.
    read_buffer = NULL;
    num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer, &num_bytes, false));
    EXPECT_TRUE(read_buffer != NULL);
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(456, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerEndReadData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Adding a waiter should now succeed.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_WAIT_FLAG_READABLE, 56));

    // Close the producer.
    dp->ProducerClose();

    // Should be never-readable.
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000));
    dp->ConsumerRemoveWaiter(&waiter);

    dp->ConsumerClose();
  }
}

void Seq(int32_t start, size_t count, int32_t* out) {
  for (size_t i = 0; i < count; i++)
    out[i] = start + static_cast<int32_t>(i);
}

TEST(LocalDataPipeTest, MayDiscard) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  int32_t buffer[100] = { 0 };
  uint32_t num_bytes = 0;

  num_bytes = 20u * sizeof(int32_t);
  Seq(0, arraysize(buffer), buffer);
  // Try writing more than capacity. (This test relies on the implementation
  // enforcing the capacity strictly.)
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read half of what we wrote.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(0, 5u, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));
  // Internally, a circular buffer would now look like:
  //   -, -, -, -, -, 5, 6, 7, 8, 9

  // Write a bit more than the space that's available.
  num_bytes = 8u * sizeof(int32_t);
  Seq(100, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(8u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   100, 101, 102, 103, 104, 105, 106, 107, 8, 9

  // Read half of what's available.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 8;
  expected_buffer[1] = 9;
  expected_buffer[2] = 100;
  expected_buffer[3] = 101;
  expected_buffer[4] = 102;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));
  // Internally, a circular buffer would now look like:
  //   -, -, -, 103, 104, 105, 106, 107, -, -

  // Write one integer.
  num_bytes = 1u * sizeof(int32_t);
  Seq(200, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   -, -, -, 103, 104, 105, 106, 107, 200, -

  // Write five more.
  num_bytes = 5u * sizeof(int32_t);
  Seq(300, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   301, 302, 303, 304, 104, 105, 106, 107, 200, 300

  // Read it all.
  num_bytes = sizeof(buffer);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 104;
  expected_buffer[1] = 105;
  expected_buffer[2] = 106;
  expected_buffer[3] = 107;
  expected_buffer[4] = 200;
  expected_buffer[5] = 300;
  expected_buffer[6] = 301;
  expected_buffer[7] = 302;
  expected_buffer[8] = 303;
  expected_buffer[9] = 304;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // TODO(vtl): Test two-phase write when it supports "may discard".

  dp->ProducerClose();
  dp->ConsumerClose();
}

// TODO(vtl): More "all or none" tests (without and with "may discard").

// Tests that |ProducerWriteData()| and |ConsumerReadData()| writes and reads,
// respectively, as much as possible, even if it has to "wrap around" the
// internal circular buffer. (Note that the two-phase write and read do not do
// this.)
TEST(LocalDataPipeTest, WrapAround) {
  unsigned char test_data[1000];
  for (size_t i = 0; i < arraysize(test_data); i++)
    test_data[i] = static_cast<unsigned char>(i);

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    1u,  // |element_num_bytes|.
    100u  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));
  // This test won't be valid if |ValidateOptions()| decides to give the pipe
  // more space.
  ASSERT_EQ(100u, validated_options.capacity_num_bytes);

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Write 20 bytes.
  uint32_t num_bytes = 20u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&test_data[0], &num_bytes, false));
  EXPECT_EQ(20u, num_bytes);

  // Read 10 bytes.
  unsigned char read_buffer[1000] = { 0 };
  num_bytes = 10u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(read_buffer, &num_bytes, false));
  EXPECT_EQ(10u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[0], 10u));

  // Check that a two-phase write can now only write (at most) 80 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  void* write_buffer_ptr = NULL;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
  EXPECT_TRUE(write_buffer_ptr != NULL);
  EXPECT_EQ(80u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Write as much data as we can (using |ProducerWriteData()|. We should write
  // 90 bytes.
  num_bytes = 200u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&test_data[20], &num_bytes, false));
  EXPECT_EQ(90u, num_bytes);

  // Check that a two-phase read can now only read (at most) 90 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  const void* read_buffer_ptr = NULL;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
  EXPECT_TRUE(read_buffer_ptr != NULL);
  EXPECT_EQ(90u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // Read as much as possible (using |ConsumerReadData()|. We should read 100
  // bytes.
  num_bytes =
      static_cast<uint32_t>(arraysize(read_buffer) * sizeof(read_buffer[0]));
  memset(read_buffer, 0, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(read_buffer, &num_bytes, false));
  EXPECT_EQ(100u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[10], 100u));

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Tests the behavior of closing the producer or consumer with respect to
// writes and reads (simple and two-phase).
TEST(LocalDataPipeTest, CloseWriteRead) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    1u,  // |element_num_bytes|.
    1000u  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&options, &validated_options));

  // Close producer first, then consumer.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Write it again, so we'll have something left over.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    EXPECT_GT(num_bytes, 0u);

    // Start two-phase read.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(2u * kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // The consumer can finish its two-phase read.
    EXPECT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(kTestDataSize));

    // And start another.
    read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer, which cancels the two-phase read.
    dp->ConsumerClose();
  }

  // Close consumer first, then producer.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    ASSERT_GT(num_bytes, kTestDataSize);

    // Start two-phase read.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer.
    dp->ConsumerClose();

    // Actually write some data. (Note: Premature freeing of the buffer would
    // probably only be detected under ASAN or similar.)
    memcpy(write_buffer_ptr, kTestData, kTestDataSize);
    // Note: Even though the consumer has been closed, ending the two-phase
    // write will report success.
    EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(kTestDataSize));

    // But trying to write should result in failure.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerWriteData(kTestData, &num_bytes, false));

    // As will trying to start another two-phase write.
    write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));

    dp->ProducerClose();
  }

  // Test closing the consumer first, then the producer, with an active
  // two-phase write.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    uint32_t num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    ASSERT_GT(num_bytes, kTestDataSize);

    dp->ConsumerClose();
    dp->ProducerClose();
  }

  // Test closing the producer and then trying to read (with no data).
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // Read that data.
    char buffer[1000];
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(buffer, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // A second read should fail.
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerReadData(buffer, &num_bytes, false));

    // A two-phase read should also fail.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));

    // Ditto for discard.
    num_bytes = 10u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerDiscardData(&num_bytes, false));

    dp->ConsumerClose();
  }
}

// TODO(vtl): More.

}  // namespace
}  // namespace system
}  // namespace mojo
