// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_data_pipe.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/data_pipe.h"
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

  // TODO(vtl): More: discard (with/without "all or none"). More "all or none"
  // tests.

  dp->ProducerClose();
  dp->ConsumerClose();
}

// TODO(vtl): More.

}  // namespace
}  // namespace system
}  // namespace mojo
