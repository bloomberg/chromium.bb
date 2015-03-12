// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

const size_t kSize = 1024;

// Test Lock() and Unlock() functionalities.
TEST(DiscardableMemoryTest, LockAndUnLock) {
  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
  void* addr = memory->Memory();
  EXPECT_NE(nullptr, addr);
  memory->Unlock();
}

// Test delete a discardable memory while it is locked.
TEST(DiscardableMemoryTest, DeleteWhileLocked) {
  const scoped_ptr<DiscardableMemory> memory(
      DiscardableMemory::CreateLockedMemory(kSize));
  ASSERT_TRUE(memory);
}

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

// Test behavior when creating enough instances that could use up a 32-bit
// address space.
// This is disabled under AddressSanitizer on Windows as it crashes (by design)
// on OOM. See http://llvm.org/PR22026 for the details.
#if !defined(ADDRESS_SANITIZER) || !defined(OS_WIN)
TEST(DiscardableMemoryTest, AddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;  // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  scoped_ptr<DiscardableMemory> instances[kNumberOfInstances];
  for (auto& memory : instances) {
    memory = DiscardableMemory::CreateLockedMemory(kLargeSize);
    ASSERT_TRUE(memory);
    void* addr = memory->Memory();
    EXPECT_NE(nullptr, addr);
    memory->Unlock();
  }
}
#endif

}  // namespace
}  // namespace base
