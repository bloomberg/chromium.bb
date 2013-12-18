// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_data_pipe.h"

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
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            dp->ConsumerReadData(elements, &num_bytes, false));

  // Query; nothing there yet.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Discard; nothing there yet.
  num_bytes = static_cast<uint32_t>(5u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, dp->ConsumerDiscardData(&num_bytes, false));

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

// TODO(vtl): More: discard (with/without "all or none"). More "all or none"
// tests.

// TODO(vtl): More.

}  // namespace
}  // namespace system
}  // namespace mojo
