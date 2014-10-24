// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/discardable_shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestDiscardableSharedMemory : public DiscardableSharedMemory {
 public:
  TestDiscardableSharedMemory() {}

  explicit TestDiscardableSharedMemory(SharedMemoryHandle handle)
      : DiscardableSharedMemory(handle) {}

  void SetNow(Time now) { now_ = now; }

 private:
  // Overriden from DiscardableSharedMemory:
  virtual Time Now() const override { return now_; }

  Time now_;
};

TEST(DiscardableSharedMemoryTest, CreateAndMap) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory;
  bool rv = memory.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);
  EXPECT_GE(memory.mapped_size(), kDataSize);
}

TEST(DiscardableSharedMemoryTest, CreateFromHandle) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  SharedMemoryHandle shared_handle;
  ASSERT_TRUE(
      memory1.ShareToProcess(GetCurrentProcessHandle(), &shared_handle));
  ASSERT_TRUE(SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory2(shared_handle);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);
}

TEST(DiscardableSharedMemoryTest, LockAndUnlock) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  // Memory is initially locked. Unlock it.
  memory1.SetNow(Time::FromDoubleT(1));
  memory1.Unlock();

  // Lock and unlock memory.
  rv = memory1.Lock();
  EXPECT_TRUE(rv);
  memory1.SetNow(Time::FromDoubleT(2));
  memory1.Unlock();

  SharedMemoryHandle shared_handle;
  ASSERT_TRUE(
      memory1.ShareToProcess(GetCurrentProcessHandle(), &shared_handle));
  ASSERT_TRUE(SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory2(shared_handle);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // Lock first instance again.
  rv = memory1.Lock();
  EXPECT_TRUE(rv);

  // Unlock second instance.
  memory2.SetNow(Time::FromDoubleT(3));
  memory2.Unlock();

  // Lock and unlock second instance.
  rv = memory2.Lock();
  EXPECT_TRUE(rv);
  memory2.SetNow(Time::FromDoubleT(4));
  memory2.Unlock();

  // Try to lock first instance again. Should fail as first instance has an
  // incorrect last know usage time.
  rv = memory1.Lock();
  EXPECT_FALSE(rv);

  // Memory should still be resident.
  rv = memory1.IsMemoryResident();
  EXPECT_TRUE(rv);

  // Second attempt to lock first instance should succeed as last known usage
  // time is now correct.
  rv = memory1.Lock();
  EXPECT_TRUE(rv);
  memory1.SetNow(Time::FromDoubleT(5));
  memory1.Unlock();
}

TEST(DiscardableSharedMemoryTest, Purge) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  SharedMemoryHandle shared_handle;
  ASSERT_TRUE(
      memory1.ShareToProcess(GetCurrentProcessHandle(), &shared_handle));
  ASSERT_TRUE(SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory2(shared_handle);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  // This should fail as memory is locked.
  rv = memory1.Purge(Time::FromDoubleT(1));
  EXPECT_FALSE(rv);

  memory2.SetNow(Time::FromDoubleT(2));
  memory2.Unlock();

  ASSERT_TRUE(memory2.IsMemoryResident());

  // Memory is unlocked, but our usage timestamp is incorrect.
  rv = memory1.Purge(Time::FromDoubleT(3));
  EXPECT_FALSE(rv);

  ASSERT_TRUE(memory2.IsMemoryResident());

  // Memory is unlocked and our usage timestamp should be correct.
  rv = memory1.Purge(Time::FromDoubleT(4));
  EXPECT_TRUE(rv);

  // Lock should fail as memory has been purged.
  rv = memory2.Lock();
  EXPECT_FALSE(rv);

  ASSERT_FALSE(memory2.IsMemoryResident());
}

TEST(DiscardableSharedMemoryTest, LastUsed) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  SharedMemoryHandle shared_handle;
  ASSERT_TRUE(
      memory1.ShareToProcess(GetCurrentProcessHandle(), &shared_handle));
  ASSERT_TRUE(SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory2(shared_handle);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  memory2.SetNow(Time::FromDoubleT(1));
  memory2.Unlock();

  EXPECT_EQ(memory2.last_known_usage(), Time::FromDoubleT(1));

  rv = memory2.Lock();
  EXPECT_TRUE(rv);

  // This should fail as memory is locked.
  rv = memory1.Purge(Time::FromDoubleT(2));
  ASSERT_FALSE(rv);

  // Last usage should have been updated to timestamp passed to Purge above.
  EXPECT_EQ(memory1.last_known_usage(), Time::FromDoubleT(2));

  memory2.SetNow(Time::FromDoubleT(3));
  memory2.Unlock();

  // Usage time should be correct for |memory2| instance.
  EXPECT_EQ(memory2.last_known_usage(), Time::FromDoubleT(3));

  // However, usage time has not changed as far as |memory1| instance knows.
  EXPECT_EQ(memory1.last_known_usage(), Time::FromDoubleT(2));

  // Memory is unlocked, but our usage timestamp is incorrect.
  rv = memory1.Purge(Time::FromDoubleT(4));
  EXPECT_FALSE(rv);

  // The failed purge attempt should have updated usage time to the correct
  // value.
  EXPECT_EQ(memory1.last_known_usage(), Time::FromDoubleT(3));

  // Purge memory through |memory2| instance. The last usage time should be
  // set to 0 as a result of this.
  rv = memory2.Purge(Time::FromDoubleT(5));
  EXPECT_TRUE(rv);
  EXPECT_TRUE(memory2.last_known_usage().is_null());

  // This should fail as memory has already been purged and |memory1|'s usage
  // time is incorrect as a result.
  rv = memory1.Purge(Time::FromDoubleT(6));
  EXPECT_FALSE(rv);

  // The failed purge attempt should have updated usage time to the correct
  // value.
  EXPECT_TRUE(memory1.last_known_usage().is_null());

  // Purge should succeed now that usage time is correct.
  rv = memory1.Purge(Time::FromDoubleT(7));
  EXPECT_TRUE(rv);
}

TEST(DiscardableSharedMemoryTest, LockShouldAlwaysFailAfterSuccessfulPurge) {
  const uint32 kDataSize = 1024;

  TestDiscardableSharedMemory memory1;
  bool rv = memory1.CreateAndMap(kDataSize);
  ASSERT_TRUE(rv);

  SharedMemoryHandle shared_handle;
  ASSERT_TRUE(
      memory1.ShareToProcess(GetCurrentProcessHandle(), &shared_handle));
  ASSERT_TRUE(SharedMemory::IsHandleValid(shared_handle));

  TestDiscardableSharedMemory memory2(shared_handle);
  rv = memory2.Map(kDataSize);
  ASSERT_TRUE(rv);

  memory2.SetNow(Time::FromDoubleT(1));
  memory2.Unlock();

  rv = memory2.Purge(Time::FromDoubleT(2));
  EXPECT_TRUE(rv);

  // Lock should fail as memory has been purged.
  rv = memory2.Lock();
  EXPECT_FALSE(rv);
  rv = memory1.Lock();
  EXPECT_FALSE(rv);
}

}  // namespace
}  // namespace base
