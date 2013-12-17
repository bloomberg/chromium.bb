// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe.h"

#include <limits>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// Does a cursory sanity check of |validated_options|. Calls |ValidateOptions()|
// on already-validated options. The validated options should be valid, and the
// revalidated copy should be the same.
void RevalidateOptions(const MojoCreateDataPipeOptions& validated_options) {
  EXPECT_EQ(kSizeOfOptions, validated_options.struct_size);
  // Nothing to check for flags.
  EXPECT_GT(validated_options.element_num_bytes, 0u);
  EXPECT_GT(validated_options.capacity_num_bytes, 0u);
  EXPECT_EQ(0u,
            validated_options.capacity_num_bytes %
                validated_options.element_num_bytes);

  MojoCreateDataPipeOptions revalidated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateOptions(&validated_options,
                                      &revalidated_options));
  EXPECT_EQ(validated_options.struct_size, revalidated_options.struct_size);
  EXPECT_EQ(validated_options.element_num_bytes,
            revalidated_options.element_num_bytes);
  EXPECT_EQ(validated_options.capacity_num_bytes,
            revalidated_options.capacity_num_bytes);
  EXPECT_EQ(validated_options.flags, revalidated_options.flags);
}

// Tests valid inputs to |ValidateOptions()|.
TEST(DataPipeTest, ValidateOptionsValidInputs) {
  // Default options.
  {
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateOptions(NULL, &validated_options));
    RevalidateOptions(validated_options);
  }

  // Different flags.
  MojoCreateDataPipeOptionsFlags flags_values[] = {
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD
  };
  for (size_t i = 0; i < arraysize(flags_values); i++) {
    const MojoCreateDataPipeOptionsFlags flags = flags_values[i];

    // Different capacities (size 1).
    for (uint32_t capacity = 1; capacity <= 100 * 1000 * 1000; capacity *= 10) {
      MojoCreateDataPipeOptions options = {
        kSizeOfOptions,  // |struct_size|.
        flags,  // |flags|.
        1,  // |element_num_bytes|.
        capacity  // |capacity_num_bytes|.
      };
      MojoCreateDataPipeOptions validated_options = { 0 };
      EXPECT_EQ(MOJO_RESULT_OK,
                DataPipe::ValidateOptions(&options, &validated_options))
          << capacity;
      RevalidateOptions(validated_options);
    }

    // Small sizes.
    for (uint32_t size = 1; size < 100; size++) {
      // Different capacities.
      for (uint32_t elements = 1; elements <= 1000 * 1000; elements *= 10) {
        MojoCreateDataPipeOptions options = {
          kSizeOfOptions,  // |struct_size|.
          flags,  // |flags|.
          size,  // |element_num_bytes|.
          size * elements  // |capacity_num_bytes|.
        };
        MojoCreateDataPipeOptions validated_options = { 0 };
        EXPECT_EQ(MOJO_RESULT_OK,
                  DataPipe::ValidateOptions(&options, &validated_options))
            << size << ", " << elements;
        RevalidateOptions(validated_options);
      }

      // Default capacity.
      {
        MojoCreateDataPipeOptions options = {
          kSizeOfOptions,  // |struct_size|.
          flags,  // |flags|.
          size,  // |element_num_bytes|.
          0  // |capacity_num_bytes|.
        };
        MojoCreateDataPipeOptions validated_options = { 0 };
        EXPECT_EQ(MOJO_RESULT_OK,
                  DataPipe::ValidateOptions(&options, &validated_options))
            << size;
        RevalidateOptions(validated_options);
      }
    }

    // Larger sizes.
    for (uint32_t size = 100; size <= 100 * 1000; size *= 10) {
      // Capacity of 1000 elements.
      {
        MojoCreateDataPipeOptions options = {
          kSizeOfOptions,  // |struct_size|.
          flags,  // |flags|.
          size,  // |element_num_bytes|.
          1000 * size  // |capacity_num_bytes|.
        };
        MojoCreateDataPipeOptions validated_options = { 0 };
        EXPECT_EQ(MOJO_RESULT_OK,
                  DataPipe::ValidateOptions(&options, &validated_options))
            << size;
        RevalidateOptions(validated_options);
      }

      // Default capacity.
      {
        MojoCreateDataPipeOptions options = {
          kSizeOfOptions,  // |struct_size|.
          flags,  // |flags|.
          size,  // |element_num_bytes|.
          0  // |capacity_num_bytes|.
        };
        MojoCreateDataPipeOptions validated_options = { 0 };
        EXPECT_EQ(MOJO_RESULT_OK,
                  DataPipe::ValidateOptions(&options, &validated_options))
            << size;
        RevalidateOptions(validated_options);
      }
    }
  }
}

TEST(DataPipeTest, ValidateOptionsInvalidInputs) {
  // Invalid |struct_size|.
  // Note: If/when we extend |MojoCreateDataPipeOptions|, this will have to be
  // updated
  for (uint32_t struct_size = 0; struct_size < kSizeOfOptions; struct_size++) {
    MojoCreateDataPipeOptions options = {
      struct_size,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1,  // |element_num_bytes|.
      1000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              DataPipe::ValidateOptions(&options, &unused));
  }

  // Invalid |element_num_bytes|.
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      0,  // |element_num_bytes|.
      1000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              DataPipe::ValidateOptions(&options, &unused));
  }
  // |element_num_bytes| too big.
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      std::numeric_limits<uint32_t>::max(),  // |element_num_bytes|.
      std::numeric_limits<uint32_t>::max()  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              DataPipe::ValidateOptions(&options, &unused));
  }
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      std::numeric_limits<uint32_t>::max() - 1000,  // |element_num_bytes|.
      std::numeric_limits<uint32_t>::max() - 1000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              DataPipe::ValidateOptions(&options, &unused));
  }

  // Invalid |capacity_num_bytes|.
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      2,  // |element_num_bytes|.
      1  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              DataPipe::ValidateOptions(&options, &unused));
  }
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      2,  // |element_num_bytes|.
      111  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              DataPipe::ValidateOptions(&options, &unused));
  }
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      5,  // |element_num_bytes|.
      104  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              DataPipe::ValidateOptions(&options, &unused));
  }
  // |capacity_num_bytes| too big.
  {
    MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      8,  // |element_num_bytes|.
      0xffff0000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
              DataPipe::ValidateOptions(&options, &unused));
  }
}

}  // namespace
}  // namespace system
}  // namespace mojo
