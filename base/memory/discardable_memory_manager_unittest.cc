// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestAllocationImpl : public internal::DiscardableMemoryManagerAllocation {
 public:
  TestAllocationImpl() : is_allocated_(false), is_locked_(false) {}
  virtual ~TestAllocationImpl() { DCHECK(!is_locked_); }

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  virtual bool AllocateAndAcquireLock(size_t bytes) OVERRIDE {
    bool was_allocated = is_allocated_;
    is_allocated_ = true;
    DCHECK(!is_locked_);
    is_locked_ = true;
    return was_allocated;
  }
  virtual void ReleaseLock() OVERRIDE {
    DCHECK(is_locked_);
    is_locked_ = false;
  }
  virtual void Purge() OVERRIDE {
    DCHECK(is_allocated_);
    is_allocated_ = false;
  }

  bool is_locked() const { return is_locked_; }

 private:
  bool is_allocated_;
  bool is_locked_;
};

class DiscardableMemoryManagerTestBase {
 public:
  DiscardableMemoryManagerTestBase() {
    manager_.RegisterMemoryPressureListener();
  }

 protected:
  enum LockStatus {
    LOCK_STATUS_FAILED,
    LOCK_STATUS_PURGED,
    LOCK_STATUS_SUCCESS
  };

  size_t BytesAllocated() const { return manager_.GetBytesAllocatedForTest(); }

  void SetMemoryLimit(size_t bytes) { manager_.SetMemoryLimit(bytes); }

  void SetBytesToKeepUnderModeratePressure(size_t bytes) {
    manager_.SetBytesToKeepUnderModeratePressure(bytes);
  }

  void Register(TestAllocationImpl* allocation, size_t bytes) {
    manager_.Register(allocation, bytes);
  }

  void Unregister(TestAllocationImpl* allocation) {
    manager_.Unregister(allocation);
  }

  bool IsRegistered(TestAllocationImpl* allocation) const {
    return manager_.IsRegisteredForTest(allocation);
  }

  LockStatus Lock(TestAllocationImpl* allocation) {
    bool purged;
    if (!manager_.AcquireLock(allocation, &purged))
      return LOCK_STATUS_FAILED;
    return purged ? LOCK_STATUS_PURGED : LOCK_STATUS_SUCCESS;
  }

  void Unlock(TestAllocationImpl* allocation) {
    manager_.ReleaseLock(allocation);
  }

  LockStatus RegisterAndLock(TestAllocationImpl* allocation, size_t bytes) {
    manager_.Register(allocation, bytes);
    return Lock(allocation);
  }

  bool CanBePurged(TestAllocationImpl* allocation) const {
    return manager_.CanBePurgedForTest(allocation);
  }

 private:
  MessageLoopForIO message_loop_;
  internal::DiscardableMemoryManager manager_;
};

class DiscardableMemoryManagerTest : public DiscardableMemoryManagerTestBase,
                                     public testing::Test {
 public:
  DiscardableMemoryManagerTest() {}
};

TEST_F(DiscardableMemoryManagerTest, CreateAndLock) {
  size_t size = 1024;
  TestAllocationImpl allocation;
  Register(&allocation, size);
  EXPECT_TRUE(IsRegistered(&allocation));
  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(&allocation));
  EXPECT_TRUE(allocation.is_locked());
  EXPECT_EQ(1024u, BytesAllocated());
  EXPECT_FALSE(CanBePurged(&allocation));
  Unlock(&allocation);
  Unregister(&allocation);
}

TEST_F(DiscardableMemoryManagerTest, CreateZeroSize) {
  size_t size = 0;
  TestAllocationImpl allocation;
  Register(&allocation, size);
  EXPECT_TRUE(IsRegistered(&allocation));
  EXPECT_EQ(LOCK_STATUS_FAILED, Lock(&allocation));
  EXPECT_EQ(0u, BytesAllocated());
  Unregister(&allocation);
}

TEST_F(DiscardableMemoryManagerTest, LockAfterUnlock) {
  size_t size = 1024;
  TestAllocationImpl allocation;
  RegisterAndLock(&allocation, size);
  EXPECT_EQ(1024u, BytesAllocated());
  EXPECT_FALSE(CanBePurged(&allocation));

  // Now unlock so we can lock later.
  Unlock(&allocation);
  EXPECT_TRUE(CanBePurged(&allocation));

  EXPECT_EQ(LOCK_STATUS_SUCCESS, Lock(&allocation));
  EXPECT_FALSE(CanBePurged(&allocation));
  Unlock(&allocation);
  Unregister(&allocation);
}

TEST_F(DiscardableMemoryManagerTest, LockAfterPurge) {
  size_t size = 1024;
  TestAllocationImpl allocation;
  RegisterAndLock(&allocation, size);
  EXPECT_EQ(1024u, BytesAllocated());
  EXPECT_FALSE(CanBePurged(&allocation));

  // Now unlock so we can lock later.
  Unlock(&allocation);
  EXPECT_TRUE(CanBePurged(&allocation));

  // Force the system to purge.
  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_CRITICAL);

  // Required because ObserverListThreadSafe notifies via PostTask.
  RunLoop().RunUntilIdle();

  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(&allocation));
  EXPECT_FALSE(CanBePurged(&allocation));

  Unlock(&allocation);
  Unregister(&allocation);
}

TEST_F(DiscardableMemoryManagerTest, LockAfterPurgeAndCannotReallocate) {
  size_t size = 1024;
  TestAllocationImpl allocation;
  RegisterAndLock(&allocation, size);
  EXPECT_EQ(1024u, BytesAllocated());
  EXPECT_FALSE(CanBePurged(&allocation));

  // Now unlock so we can lock later.
  Unlock(&allocation);
  EXPECT_TRUE(CanBePurged(&allocation));

  // Set max allowed allocation to 1 byte. This will cause the memory to be
  // purged.
  SetMemoryLimit(1);

  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(&allocation));
  EXPECT_FALSE(CanBePurged(&allocation));

  Unlock(&allocation);
  Unregister(&allocation);
}

TEST_F(DiscardableMemoryManagerTest, Overflow) {
  size_t size = 1024;
  {
    TestAllocationImpl allocation;
    RegisterAndLock(&allocation, size);
    EXPECT_EQ(1024u, BytesAllocated());

    size_t massive_size = std::numeric_limits<size_t>::max();
    TestAllocationImpl massive_allocation;
    Register(&massive_allocation, massive_size);
    EXPECT_EQ(LOCK_STATUS_FAILED, Lock(&massive_allocation));
    EXPECT_EQ(1024u, BytesAllocated());

    Unlock(&allocation);
    EXPECT_EQ(LOCK_STATUS_PURGED, Lock(&massive_allocation));
    Unlock(&massive_allocation);
    Unregister(&massive_allocation);
    Unregister(&allocation);
  }
  EXPECT_EQ(0u, BytesAllocated());
}

class PermutationTestData {
 public:
  PermutationTestData(unsigned d0, unsigned d1, unsigned d2) {
    ordering_[0] = d0;
    ordering_[1] = d1;
    ordering_[2] = d2;
  }

  const unsigned* ordering() const { return ordering_; }

 private:
  unsigned ordering_[3];
};

class DiscardableMemoryManagerPermutationTest
    : public DiscardableMemoryManagerTestBase,
      public testing::TestWithParam<PermutationTestData> {
 public:
  DiscardableMemoryManagerPermutationTest() {}

 protected:
  // Use memory in order specified by ordering parameter.
  void RegisterAndUseAllocations() {
    for (int i = 0; i < 3; ++i) {
      RegisterAndLock(&allocation_[i], 1024);
      Unlock(&allocation_[i]);
    }
    for (int i = 0; i < 3; ++i) {
      int index = GetParam().ordering()[i];
      EXPECT_NE(LOCK_STATUS_FAILED, Lock(&allocation_[index]));
      // Leave i == 0 locked.
      if (i > 0)
        Unlock(&allocation_[index]);
    }
  }

  TestAllocationImpl* allocation(unsigned position) {
    return &allocation_[GetParam().ordering()[position]];
  }

  void UnlockAndUnregisterAllocations() {
    for (int i = 0; i < 3; ++i) {
      if (allocation_[i].is_locked())
        Unlock(&allocation_[i]);
      Unregister(&allocation_[i]);
    }
  }

 private:
  TestAllocationImpl allocation_[3];
};

// Verify that memory was discarded in the correct order after applying
// memory pressure.
TEST_P(DiscardableMemoryManagerPermutationTest, LRUDiscardedModeratePressure) {
  RegisterAndUseAllocations();

  SetBytesToKeepUnderModeratePressure(1024);
  SetMemoryLimit(2048);

  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_MODERATE);
  RunLoop().RunUntilIdle();

  EXPECT_NE(LOCK_STATUS_FAILED, Lock(allocation(2)));
  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(allocation(1)));
  // 0 should still be locked.
  EXPECT_TRUE(allocation(0)->is_locked());

  UnlockAndUnregisterAllocations();
}

// Verify that memory was discarded in the correct order after changing
// memory limit.
TEST_P(DiscardableMemoryManagerPermutationTest, LRUDiscardedExceedLimit) {
  RegisterAndUseAllocations();

  SetBytesToKeepUnderModeratePressure(1024);
  SetMemoryLimit(2048);

  EXPECT_NE(LOCK_STATUS_FAILED, Lock(allocation(2)));
  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(allocation(1)));
  // 0 should still be locked.
  EXPECT_TRUE(allocation(0)->is_locked());

  UnlockAndUnregisterAllocations();
}

// Verify that no more memory than necessary was discarded after changing
// memory limit.
TEST_P(DiscardableMemoryManagerPermutationTest, LRUDiscardedAmount) {
  SetBytesToKeepUnderModeratePressure(2048);
  SetMemoryLimit(4096);

  RegisterAndUseAllocations();

  SetMemoryLimit(2048);

  EXPECT_EQ(LOCK_STATUS_SUCCESS, Lock(allocation(2)));
  EXPECT_EQ(LOCK_STATUS_PURGED, Lock(allocation(1)));
  // 0 should still be locked.
  EXPECT_TRUE(allocation(0)->is_locked());

  UnlockAndUnregisterAllocations();
}

TEST_P(DiscardableMemoryManagerPermutationTest, PurgeFreesAllUnlocked) {
  RegisterAndUseAllocations();

  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_CRITICAL);
  RunLoop().RunUntilIdle();

  for (int i = 0; i < 3; ++i) {
    if (i == 0)
      EXPECT_TRUE(allocation(i)->is_locked());
    else
      EXPECT_EQ(LOCK_STATUS_PURGED, Lock(allocation(i)));
  }

  UnlockAndUnregisterAllocations();
}

INSTANTIATE_TEST_CASE_P(DiscardableMemoryManagerPermutationTests,
                        DiscardableMemoryManagerPermutationTest,
                        ::testing::Values(PermutationTestData(0, 1, 2),
                                          PermutationTestData(0, 2, 1),
                                          PermutationTestData(1, 0, 2),
                                          PermutationTestData(1, 2, 0),
                                          PermutationTestData(2, 0, 1),
                                          PermutationTestData(2, 1, 0)));

TEST_F(DiscardableMemoryManagerTest, NormalDestruction) {
  {
    size_t size = 1024;
    TestAllocationImpl allocation;
    Register(&allocation, size);
    Unregister(&allocation);
  }
  EXPECT_EQ(0u, BytesAllocated());
}

TEST_F(DiscardableMemoryManagerTest, DestructionAfterLocked) {
  {
    size_t size = 1024;
    TestAllocationImpl allocation;
    RegisterAndLock(&allocation, size);
    EXPECT_EQ(1024u, BytesAllocated());
    EXPECT_FALSE(CanBePurged(&allocation));
    Unlock(&allocation);
    Unregister(&allocation);
  }
  EXPECT_EQ(0u, BytesAllocated());
}

TEST_F(DiscardableMemoryManagerTest, DestructionAfterPurged) {
  {
    size_t size = 1024;
    TestAllocationImpl allocation;
    RegisterAndLock(&allocation, size);
    EXPECT_EQ(1024u, BytesAllocated());
    Unlock(&allocation);
    EXPECT_TRUE(CanBePurged(&allocation));
    SetMemoryLimit(0);
    EXPECT_EQ(0u, BytesAllocated());
    Unregister(&allocation);
  }
  EXPECT_EQ(0u, BytesAllocated());
}

class ThreadedDiscardableMemoryManagerTest
    : public DiscardableMemoryManagerTest {
 public:
  ThreadedDiscardableMemoryManagerTest()
      : memory_usage_thread_("memory_usage_thread"),
        thread_sync_(true, false) {}

  virtual void SetUp() OVERRIDE { memory_usage_thread_.Start(); }

  virtual void TearDown() OVERRIDE { memory_usage_thread_.Stop(); }

  void UseMemoryHelper() {
    size_t size = 1024;
    TestAllocationImpl allocation;
    RegisterAndLock(&allocation, size);
    Unlock(&allocation);
    Unregister(&allocation);
  }

  void SignalHelper() { thread_sync_.Signal(); }

  Thread memory_usage_thread_;
  WaitableEvent thread_sync_;
};

TEST_F(ThreadedDiscardableMemoryManagerTest, UseMemoryOnThread) {
  memory_usage_thread_.message_loop()->PostTask(
      FROM_HERE,
      Bind(&ThreadedDiscardableMemoryManagerTest::UseMemoryHelper,
           Unretained(this)));
  memory_usage_thread_.message_loop()->PostTask(
      FROM_HERE,
      Bind(&ThreadedDiscardableMemoryManagerTest::SignalHelper,
           Unretained(this)));
  thread_sync_.Wait();
}

}  // namespace
}  // namespace base
