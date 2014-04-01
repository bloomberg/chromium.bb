// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/shared_buffer_dispatcher.h"

#include <limits>

#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/raw_shared_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

// NOTE(vtl): There's currently not much to test for in
// |SharedBufferDispatcher::ValidateOptions()|, but the tests should be expanded
// if/when options are added, so I've kept the general form of the tests from
// data_pipe_unittest.cc.

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateSharedBufferOptions));

// Does a cursory sanity check of |validated_options|. Calls |ValidateOptions()|
// on already-validated options. The validated options should be valid, and the
// revalidated copy should be the same.
void RevalidateOptions(const MojoCreateSharedBufferOptions& validated_options) {
  EXPECT_EQ(kSizeOfOptions, validated_options.struct_size);
  // Nothing to check for flags.

  MojoCreateSharedBufferOptions revalidated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateOptions(&validated_options,
                                                    &revalidated_options));
  EXPECT_EQ(validated_options.struct_size, revalidated_options.struct_size);
  EXPECT_EQ(validated_options.flags, revalidated_options.flags);
}

// Tests valid inputs to |ValidateOptions()|.
TEST(SharedBufferDispatcherTest, ValidateOptionsValidInputs) {
  // Default options.
  {
    MojoCreateSharedBufferOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              SharedBufferDispatcher::ValidateOptions(NULL,
                                                      &validated_options));
    RevalidateOptions(validated_options);
  }

  // Different flags.
  MojoCreateSharedBufferOptionsFlags flags_values[] = {
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE
  };
  for (size_t i = 0; i < arraysize(flags_values); i++) {
    const MojoCreateSharedBufferOptionsFlags flags = flags_values[i];

    // Different capacities (size 1).
    for (uint32_t capacity = 1; capacity <= 100 * 1000 * 1000; capacity *= 10) {
      MojoCreateSharedBufferOptions options = {
        kSizeOfOptions,  // |struct_size|.
        flags  // |flags|.
      };
      MojoCreateSharedBufferOptions validated_options = { 0 };
      EXPECT_EQ(MOJO_RESULT_OK,
                SharedBufferDispatcher::ValidateOptions(&options,
                                                        &validated_options))
          << capacity;
      RevalidateOptions(validated_options);
    }
  }
}

TEST(SharedBufferDispatcherTest, ValidateOptionsInvalidInputs) {
  // Invalid |struct_size|.
  // Note: If/when we extend |MojoCreateSharedBufferOptions|, this will have to
  // be updated.
  for (uint32_t struct_size = 0; struct_size < kSizeOfOptions; struct_size++) {
    MojoCreateSharedBufferOptions options = {
      struct_size,  // |struct_size|.
      MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE  // |flags|.
    };
    MojoCreateSharedBufferOptions unused = { 0 };
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              SharedBufferDispatcher::ValidateOptions(&options, &unused));
  }
}

TEST(SharedBufferDispatcherTest, CreateAndMapBuffer) {
  MojoCreateSharedBufferOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateOptions(NULL,
                                                    &validated_options));

  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::Create(validated_options, 100,
                                           &dispatcher));
  ASSERT_TRUE(dispatcher);
  EXPECT_EQ(Dispatcher::kTypeSharedBuffer, dispatcher->GetType());

  // Make a couple of mappings.
  scoped_ptr<RawSharedBufferMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE,
                                  &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->base());
  EXPECT_EQ(100u, mapping1->length());
  // Write something.
  static_cast<char*>(mapping1->base())[50] = 'x';

  scoped_ptr<RawSharedBufferMapping> mapping2;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher->MapBuffer(50, 50, MOJO_MAP_BUFFER_FLAG_NONE,
                                  &mapping2));
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping2->base());
  EXPECT_EQ(50u, mapping2->length());
  EXPECT_EQ('x', static_cast<char*>(mapping2->base())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // Check that we can still read/write to mappings after the dispatcher has
  // gone away.
  static_cast<char*>(mapping2->base())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->base())[51]);
}

TEST(SharedBufferDispatcher, DuplicateBufferHandle) {
  MojoCreateSharedBufferOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateOptions(NULL,
                                                    &validated_options));

  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::Create(validated_options, 100,
                                           &dispatcher1));

  // Map and write something.
  scoped_ptr<RawSharedBufferMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher1->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE,
                                   &mapping));
  static_cast<char*>(mapping->base())[0] = 'x';
  mapping.reset();

  // Duplicate |dispatcher1| and then close it.
  scoped_refptr<Dispatcher> dispatcher2;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher1->DuplicateBufferHandle(NULL, &dispatcher2));
  ASSERT_TRUE(dispatcher2);
  EXPECT_EQ(Dispatcher::kTypeSharedBuffer, dispatcher2->GetType());

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());

  // Map |dispatcher2| and read something.
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher2->MapBuffer(0, 100, MOJO_MAP_BUFFER_FLAG_NONE,
                                   &mapping));
  EXPECT_EQ('x', static_cast<char*>(mapping->base())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->Close());
}

// TODO(vtl): Test |DuplicateBufferHandle()| with non-null (valid) |options|.

TEST(SharedBufferDispatcherTest, CreateInvalidNumBytes) {
  MojoCreateSharedBufferOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateOptions(NULL,
                                                    &validated_options));

  // Size too big.
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            SharedBufferDispatcher::Create(validated_options,
                                           std::numeric_limits<uint64_t>::max(),
                                           &dispatcher));
  EXPECT_FALSE(dispatcher);

  // Zero size.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            SharedBufferDispatcher::Create(validated_options, 0, &dispatcher));
  EXPECT_FALSE(dispatcher);
}

TEST(SharedBufferDispatcherTest, MapBufferInvalidArguments) {
  MojoCreateSharedBufferOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateOptions(NULL,
                                                    &validated_options));

  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::Create(validated_options, 100,
                                           &dispatcher));

  scoped_ptr<RawSharedBufferMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, 101, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(1, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, 0, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

// TODO(vtl): Test |DuplicateBufferHandle()| with invalid |options|.

}  // namespace
}  // namespace system
}  // namespace mojo

