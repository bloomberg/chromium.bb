// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "third_party/ashmem/ashmem.h"
#endif

namespace base {

#if defined(OS_ANDROID)
// Test Lock() and Unlock() functionalities.
TEST(DiscardableMemoryTest, LockAndUnLock) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  size_t size = 1024;

  DiscardableMemory memory;
  ASSERT_TRUE(memory.InitializeAndLock(size));
  void* addr = memory.Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory.Unlock();
  EXPECT_EQ(DISCARDABLE_MEMORY_SUCCESS, memory.Lock());
  addr = memory.Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory.Unlock();
}

// Test delete a discardable memory while it is locked.
TEST(DiscardableMemoryTest, DeleteWhileLocked) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  size_t size = 1024;
  DiscardableMemory memory;
  ASSERT_TRUE(memory.InitializeAndLock(size));
}
#endif

}
