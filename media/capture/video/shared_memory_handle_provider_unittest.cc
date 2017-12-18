// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/shared_memory_handle_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/shared_memory.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const size_t kMemorySize = 1024;

class MojoInitializer {
 public:
  MojoInitializer() { mojo::edk::Init(); }
};

void EnsureMojoInitialized() {
  CR_DEFINE_STATIC_LOCAL(MojoInitializer, initializer, ());
  ALLOW_UNUSED_LOCAL(initializer);
}

}  // anonymous namespace

class SharedMemoryHandleProviderTest : public ::testing::Test {
 public:
  SharedMemoryHandleProviderTest() {
    // Initialize the mojo EDK, so that mojo handle can be created.
    EnsureMojoInitialized();
  }

  ~SharedMemoryHandleProviderTest() override = default;

  void UnwrapAndVerifyMojoHandle(mojo::ScopedSharedBufferHandle buffer_handle,
                                 size_t expected_size,
                                 bool expected_read_only_flag) {
    base::SharedMemoryHandle memory_handle;
    size_t memory_size = 0;
    bool read_only_flag = false;
    const MojoResult result =
        mojo::UnwrapSharedMemoryHandle(std::move(buffer_handle), &memory_handle,
                                       &memory_size, &read_only_flag);
    EXPECT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(expected_size, memory_size);
    EXPECT_EQ(expected_read_only_flag, read_only_flag);
  }

 protected:
  SharedMemoryHandleProvider handle_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryHandleProviderTest);
};

TEST_F(SharedMemoryHandleProviderTest,
       VerifyInterProcessTransitHandleForReadOnly) {
  handle_provider_.InitForSize(kMemorySize);

  auto mojo_handle =
      handle_provider_.GetHandleForInterProcessTransit(true /* read_only */);
  UnwrapAndVerifyMojoHandle(std::move(mojo_handle), kMemorySize, true);
}

TEST_F(SharedMemoryHandleProviderTest,
       VerifyInterProcessTransitHandleForReadWrite) {
  handle_provider_.InitForSize(kMemorySize);

  auto mojo_handle =
      handle_provider_.GetHandleForInterProcessTransit(false /* read_only */);
  UnwrapAndVerifyMojoHandle(std::move(mojo_handle), kMemorySize, false);
}

};  // namespace media
