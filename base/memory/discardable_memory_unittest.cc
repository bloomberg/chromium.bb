// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

const size_t kSize = 1024;

#if defined(OS_ANDROID)
TEST(DiscardableMemoryTest, TooLargeAllocationFails) {
  const size_t kPageSize = 4096;
  const size_t max_allowed_allocation_size =
      std::numeric_limits<size_t>::max() - kPageSize + 1;
  scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(max_allowed_allocation_size + 1));
  // On certain platforms (e.g. Android), page-alignment would have caused an
  // overflow resulting in a small allocation if the input size wasn't checked
  // correctly.
  ASSERT_FALSE(memory);
}
#endif

TEST(DiscardableMemoryTest, SupportedNatively) {
#if defined(DISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY)
  ASSERT_TRUE(DiscardableMemory::SupportedNatively());
#else
  // If we ever have a platform that decides at runtime if it can support
  // discardable memory natively, then we'll have to add a 'never supported
  // natively' define for this case. At present, if it's not always supported
  // natively, it's never supported.
  ASSERT_FALSE(DiscardableMemory::SupportedNatively());
#endif
}

// Test Lock() and Unlock() functionalities.
TEST(DiscardableMemoryTest, LockAndUnLock) {
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
  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
}

#if !defined(OS_ANDROID)
// Test forced purging.
TEST(DiscardableMemoryTest, Purge) {
  ASSERT_TRUE(DiscardableMemory::PurgeForTestingSupported());

  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  memory->Unlock();

  DiscardableMemory::PurgeForTesting();
  EXPECT_EQ(DISCARDABLE_MEMORY_PURGED, memory->Lock());
}
#endif  // !OS_ANDROID

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

}
