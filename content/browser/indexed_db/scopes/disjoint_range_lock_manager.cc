// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/disjoint_range_lock_manager.h"

namespace content {

DisjointRangeLockManager::LockRequest::LockRequest() = default;
DisjointRangeLockManager::LockRequest::LockRequest(LockType type,
                                                   LockAquiredCallback callback)
    : requested_type(type), callback(std::move(callback)) {}
DisjointRangeLockManager::LockRequest::LockRequest(LockRequest&&) = default;
DisjointRangeLockManager::LockRequest::~LockRequest() = default;
DisjointRangeLockManager::Lock::Lock() = default;
DisjointRangeLockManager::Lock::Lock(Lock&&) = default;
DisjointRangeLockManager::Lock::~Lock() = default;
DisjointRangeLockManager::Lock& DisjointRangeLockManager::Lock::operator=(
    DisjointRangeLockManager::Lock&&) = default;

DisjointRangeLockManager::DisjointRangeLockManager(
    int level_count,
    const leveldb::Comparator* comparator,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : comparator_(comparator), task_runner_(task_runner), weak_factory_(this) {
  for (int level = 0; level < level_count; ++level) {
    locks_.emplace_back(RangeLessThan(comparator));
  }
}

DisjointRangeLockManager::~DisjointRangeLockManager() = default;

int64_t DisjointRangeLockManager::LocksHeldForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t locks = 0;
  for (const LockLevelMap& map : locks_) {
    for (const auto& pair : map) {
      locks += pair.second.acquired_count;
    }
  }
  return locks;
}
int64_t DisjointRangeLockManager::RequestsWaitingForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t requests = 0;
  for (const LockLevelMap& map : locks_) {
    for (const auto& pair : map) {
      requests += pair.second.queue.size();
    }
  }
  return requests;
}

void DisjointRangeLockManager::AcquireLock(int level,
                                           const LockRange& range,
                                           LockType type,
                                           LockAquiredCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(level, static_cast<int>(locks_.size()));
  DCHECK_LT(comparator_->Compare(leveldb::Slice(range.begin),
                                 leveldb::Slice(range.end)),
            0)
      << "Range is invalid: " << range;

  auto& level_locks = locks_[level];

  auto it = level_locks.find(range);
  if (it == level_locks.end()) {
    it = level_locks
             .emplace(std::piecewise_construct, std::forward_as_tuple(range),
                      std::forward_as_tuple())
             .first;
  }

  DCHECK_EQ(it->first, range)
      << range << " does not match what is in the lock map " << it->first;
#if DCHECK_IS_ON()
  DCHECK(IsRangeDisjointFromNeighbors(level_locks, range, comparator_))
      << "Range not discrete: " << range;
#endif
  Lock& lock = it->second;

  if (lock.CanBeAcquired(type)) {
    ++lock.acquired_count;
    lock.lock_mode = type;
    std::move(callback).Run(
        ScopeLock(range, level,
                  base::BindOnce(&DisjointRangeLockManager::LockReleased,
                                 weak_factory_.GetWeakPtr(), level, range)));
  } else {
    lock.queue.emplace_back(type, std::move(callback));
  }
}

void DisjointRangeLockManager::RemoveLockRange(int level,
                                               const LockRange& range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(level, static_cast<int>(locks_.size()));
  auto& level_locks = locks_[level];
  auto it = level_locks.find(range);
  if (it != level_locks.end()) {
    DCHECK_EQ(0, it->second.acquired_count);
    level_locks.erase(it);
  }
}

void DisjointRangeLockManager::LockReleased(int level, LockRange range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(level, static_cast<int>(locks_.size()));
  auto& level_locks = locks_[level];
  auto it = level_locks.find(range);
  DCHECK(it != level_locks.end());
  Lock& lock = it->second;

  DCHECK_GT(lock.acquired_count, 0);
  --(lock.acquired_count);
  if (lock.acquired_count == 0) {
    // Either the lock isn't acquired yet or more shared locks can be granted.
    while (!lock.queue.empty() &&
           (lock.acquired_count == 0 ||
            lock.queue.front().requested_type == LockType::kShared)) {
      LockRequest requester = std::move(lock.queue.front());
      lock.queue.pop_front();
      ++lock.acquired_count;
      lock.lock_mode = requester.requested_type;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(requester.callback),
              ScopeLock(range, level,
                        base::BindOnce(&DisjointRangeLockManager::LockReleased,
                                       weak_factory_.GetWeakPtr(), level,
                                       std::move(range)))));
      // This can only happen if acquired_count was 0.
      if (requester.requested_type == LockType::kExclusive)
        return;
    }
  }
}

#if DCHECK_IS_ON()
// static
bool DisjointRangeLockManager::IsRangeDisjointFromNeighbors(
    const LockLevelMap& map,
    const LockRange& range,
    const leveldb::Comparator* const comparator) {
  DCHECK_EQ(map.count(range), 1ull);
  auto it = map.find(range);
  auto next_it = it;
  ++next_it;
  if (next_it != map.end()) {
    return comparator->Compare(leveldb::Slice(range.end),
                               leveldb::Slice(next_it->first.begin)) <= 0;
  }
  auto last_it = it;
  if (last_it != map.begin()) {
    --last_it;
    return comparator->Compare(leveldb::Slice(last_it->first.end),
                               leveldb::Slice(range.begin)) <= 0;
  }
  return true;
}
#endif

}  // namespace content
