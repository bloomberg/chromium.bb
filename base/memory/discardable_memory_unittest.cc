// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
const size_t kSize = 1024;

// Test Lock() and Unlock() functionalities.
TEST(DiscardableMemoryTest, LockAndUnLock) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  void* addr = memory->Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory->Unlock();
  // The system should have no reason to purge discardable blocks in this brief
  // interval, though technically speaking this might flake.
  EXPECT_EQ(DISCARDABLE_MEMORY_SUCCESS, memory->Lock());
  addr = memory->Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory->Unlock();
}

// Test delete a discardable memory while it is locked.
TEST(DiscardableMemoryTest, DeleteWhileLocked) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
}

#if defined(OS_MACOSX)
// Test forced purging.
TEST(DiscardableMemoryTest, Purge) {
  ASSERT_TRUE(DiscardableMemory::Supported());
  ASSERT_TRUE(DiscardableMemory::PurgeForTestingSupported());

  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  memory->Unlock();

  DiscardableMemory::PurgeForTesting();
  EXPECT_EQ(DISCARDABLE_MEMORY_PURGED, memory->Lock());
}
#endif  // OS_MACOSX

#if !defined(NDEBUG) && !defined(OS_ANDROID)
// Death tests are not supported with Android APKs.
TEST(DiscardableMemoryTest, UnlockedMemoryAccessCrashesInDebugMode) {
  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  memory->Unlock();
  ASSERT_DEATH_IF_SUPPORTED(
      { *static_cast<int*>(memory->Memory()) = 0xdeadbeef; }, ".*");
}
#endif

#endif  // OS_*

}
