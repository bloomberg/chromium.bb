// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/disjoint_range_lock_manager.h"

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/scopes/scope_lock.h"
#include "content/browser/indexed_db/scopes/scope_lock_range.h"
#include "content/test/barrier_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

template <typename T>
void SetValue(T* out, T value) {
  *out = value;
}

std::string IntegerKey(size_t num) {
  return base::StringPrintf("%010zd", num);
}

void StoreLocks(std::vector<ScopeLock>* locks_out,
                base::OnceClosure done,
                std::vector<ScopeLock> locks) {
  (*locks_out) = std::move(locks);
  std::move(done).Run();
}

class DisjointRangeLockManagerTest : public testing::Test {
 public:
  DisjointRangeLockManagerTest() = default;
  ~DisjointRangeLockManagerTest() override = default;

 private:
  base::test::ScopedTaskEnvironment task_env_;
};

TEST_F(DisjointRangeLockManagerTest, BasicAcquisition) {
  const size_t kTotalLocks = 10;
  DisjointRangeLockManager lock_manager(1);

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  base::RunLoop loop;
  std::vector<ScopeLock> locks1;
  std::vector<ScopeLock> locks2;
  std::vector<ScopeLock> locks;
  locks.resize(kTotalLocks);
  {
    BarrierBuilder barrier(loop.QuitClosure());

    std::vector<ScopesLockManager::ScopeLockRequest> locks1_requests;
    for (size_t i = 0; i < kTotalLocks / 2; ++i) {
      ScopeLockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      locks1_requests.emplace_back(0, std::move(range),
                                   ScopesLockManager::LockType::kExclusive);
    }
    EXPECT_TRUE(lock_manager.AcquireLocks(
        locks1_requests,
        base::BindOnce(StoreLocks, &locks1, barrier.AddClosure())));

    // Now acquire kTotalLocks/2 locks starting at (kTotalLocks-1) to verify
    // they acquire in the correct order.
    std::vector<ScopesLockManager::ScopeLockRequest> locks2_requests;
    for (size_t i = kTotalLocks - 1; i >= kTotalLocks / 2; --i) {
      ScopeLockRange range = {IntegerKey(i), IntegerKey(i + 1)};
      locks2_requests.emplace_back(0, std::move(range),
                                   ScopesLockManager::LockType::kExclusive);
    }
    EXPECT_TRUE(lock_manager.AcquireLocks(
        locks2_requests,
        base::BindOnce(StoreLocks, &locks2, barrier.AddClosure())));
  }
  loop.Run();
  EXPECT_EQ(static_cast<int64_t>(kTotalLocks),
            lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  // All locks should be acquired.
  for (const auto& lock : locks1) {
    EXPECT_TRUE(lock.is_locked());
  }
  for (const auto& lock : locks2) {
    EXPECT_TRUE(lock.is_locked());
  }

  // Release locks manually
  for (auto& lock : locks1) {
    lock.Release();
    EXPECT_FALSE(lock.is_locked());
  }
  for (auto& lock : locks2) {
    lock.Release();
    EXPECT_FALSE(lock.is_locked());
  }

  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  locks1.clear();
  locks2.clear();
}

TEST_F(DisjointRangeLockManagerTest, Shared) {
  DisjointRangeLockManager lock_manager(1);
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};

  std::vector<ScopeLock> locks1;
  std::vector<ScopeLock> locks2;
  base::RunLoop loop;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kShared}},
        base::BindOnce(StoreLocks, &locks1, barrier.AddClosure())));
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kShared}},
        base::BindOnce(StoreLocks, &locks2, barrier.AddClosure())));
  }
  loop.Run();
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());

  EXPECT_TRUE(locks1.begin()->is_locked());
  EXPECT_TRUE(locks2.begin()->is_locked());
}

TEST_F(DisjointRangeLockManagerTest, SharedAndExclusiveQueuing) {
  DisjointRangeLockManager lock_manager(1);
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};

  std::vector<ScopeLock> shared_lock1;
  std::vector<ScopeLock> shared_lock2;
  std::vector<ScopeLock> exclusive_lock3;
  std::vector<ScopeLock> shared_lock3;

  {
    base::RunLoop loop;
    {
      BarrierBuilder barrier(loop.QuitClosure());
      EXPECT_TRUE(lock_manager.AcquireLocks(
          {{0, range, ScopesLockManager::LockType::kShared}},
          base::BindOnce(StoreLocks, &shared_lock1, barrier.AddClosure())));
      EXPECT_TRUE(lock_manager.AcquireLocks(
          {{0, range, ScopesLockManager::LockType::kShared}},
          base::BindOnce(StoreLocks, &shared_lock2, barrier.AddClosure())));
    }
    loop.Run();
  }
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());

  // Both of the following locks should be queued - the exclusive is next in
  // line, then the shared lock will come after it.
  EXPECT_TRUE(lock_manager.AcquireLocks(
      {{0, range, ScopesLockManager::LockType::kExclusive}},
      base::BindOnce(StoreLocks, &exclusive_lock3, base::DoNothing::Once())));
  EXPECT_TRUE(lock_manager.AcquireLocks(
      {{0, range, ScopesLockManager::LockType::kShared}},
      base::BindOnce(StoreLocks, &shared_lock3, base::DoNothing::Once())));
  // Flush the task queue.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(exclusive_lock3.empty());
  EXPECT_TRUE(shared_lock3.empty());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(2ll, lock_manager.RequestsWaitingForTesting());

  // Release the shared locks.
  shared_lock1.clear();
  shared_lock2.clear();
  EXPECT_TRUE(shared_lock1.empty());
  EXPECT_TRUE(shared_lock2.empty());

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(exclusive_lock3.empty());
  EXPECT_TRUE(shared_lock3.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(1ll, lock_manager.RequestsWaitingForTesting());

  exclusive_lock3.clear();
  EXPECT_TRUE(exclusive_lock3.empty());

  // Flush the task queue to propagate the lock releases and grant the exclusive
  // lock.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_FALSE(shared_lock3.empty());
  EXPECT_EQ(1ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

TEST_F(DisjointRangeLockManagerTest, LevelsOperateSeparately) {
  DisjointRangeLockManager lock_manager(2);
  base::RunLoop loop;
  std::vector<ScopeLock> l0_lock;
  std::vector<ScopeLock> l1_lock;
  {
    BarrierBuilder barrier(loop.QuitClosure());
    ScopeLockRange range = {IntegerKey(0), IntegerKey(1)};
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{0, range, ScopesLockManager::LockType::kExclusive}},
        base::BindOnce(StoreLocks, &l0_lock, barrier.AddClosure())));
    EXPECT_TRUE(lock_manager.AcquireLocks(
        {{1, range, ScopesLockManager::LockType::kExclusive}},
        base::BindOnce(StoreLocks, &l1_lock, barrier.AddClosure())));
  }
  loop.Run();
  EXPECT_FALSE(l0_lock.empty());
  EXPECT_FALSE(l1_lock.empty());
  EXPECT_EQ(2ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  l0_lock.clear();
  l1_lock.clear();
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
}

TEST_F(DisjointRangeLockManagerTest, InvalidRequests) {
  DisjointRangeLockManager lock_manager(2);
  std::vector<ScopeLock> locks;
  ScopeLockRange range1 = {IntegerKey(0), IntegerKey(2)};
  ScopeLockRange range2 = {IntegerKey(1), IntegerKey(3)};
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{0, range1, ScopesLockManager::LockType::kShared},
       {0, range2, ScopesLockManager::LockType::kShared}},
      base::BindOnce(StoreLocks, &locks, base::DoNothing::Once())));
  EXPECT_TRUE(locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{-1, range1, ScopesLockManager::LockType::kShared}},
      base::BindOnce(StoreLocks, &locks, base::DoNothing::Once())));
  EXPECT_TRUE(locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{4, range1, ScopesLockManager::LockType::kShared}},
      base::BindOnce(StoreLocks, &locks, base::DoNothing::Once())));
  EXPECT_TRUE(locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
  ScopeLockRange range3 = {IntegerKey(2), IntegerKey(1)};
  EXPECT_FALSE(lock_manager.AcquireLocks(
      {{0, range1, ScopesLockManager::LockType::kShared}},
      base::BindOnce(StoreLocks, &locks, base::DoNothing::Once())));
  EXPECT_TRUE(locks.empty());
  EXPECT_EQ(0ll, lock_manager.LocksHeldForTesting());
  EXPECT_EQ(0ll, lock_manager.RequestsWaitingForTesting());
}

}  // namespace
}  // namespace content
