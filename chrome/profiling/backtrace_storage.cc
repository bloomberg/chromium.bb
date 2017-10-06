// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/backtrace_storage.h"

#include "base/logging.h"
#include "chrome/profiling/backtrace.h"

namespace profiling {

BacktraceStorage::Lock::Lock() : storage_(nullptr) {}

BacktraceStorage::Lock::Lock(BacktraceStorage* storage) : storage_(storage) {
  storage_->LockStorage();
}

BacktraceStorage::Lock::Lock(Lock&& other) : storage_(other.storage_) {
  other.storage_ = nullptr;  // Prevent the other from unlocking.
}

BacktraceStorage::Lock::~Lock() {
  if (storage_)
    storage_->UnlockStorage();
}

BacktraceStorage::Lock& BacktraceStorage::Lock::operator=(Lock&& other) {
  if (storage_)
    storage_->UnlockStorage();
  storage_ = other.storage_;
  other.storage_ = nullptr;
  return *this;
}

BacktraceStorage::BacktraceStorage() {}

BacktraceStorage::~BacktraceStorage() {}

const Backtrace* BacktraceStorage::Insert(std::vector<Address>&& bt) {
  base::AutoLock lock(lock_);

  auto iter = backtraces_.insert(Backtrace(std::move(bt))).first;
  iter->AddRef();
  return &*iter;
}

void BacktraceStorage::Free(const Backtrace* bt) {
  base::AutoLock lock(lock_);

  if (storage_lock_count_) {
    release_after_lock_.push_back(bt);
  } else {
    if (!bt->Release())
      backtraces_.erase(backtraces_.find(*bt));
  }
}

void BacktraceStorage::Free(const std::vector<const Backtrace*>& bts) {
  base::AutoLock lock(lock_);

  if (storage_lock_count_) {
    release_after_lock_.insert(release_after_lock_.end(), bts.begin(),
                               bts.end());
  } else {
    ReleaseBacktracesLocked(bts);
  }
}

void BacktraceStorage::LockStorage() {
  base::AutoLock lock(lock_);
  storage_lock_count_++;
}

void BacktraceStorage::UnlockStorage() {
  base::AutoLock lock(lock_);
  DCHECK(storage_lock_count_ > 0);
  storage_lock_count_--;

  if (storage_lock_count_ == 0) {
    ReleaseBacktracesLocked(release_after_lock_);
    release_after_lock_.clear();
    release_after_lock_.shrink_to_fit();
  }
}

void BacktraceStorage::ReleaseBacktracesLocked(
    const std::vector<const Backtrace*>& bts) {
  lock_.AssertAcquired();
  DCHECK_EQ(0, storage_lock_count_);
  for (size_t i = 0; i < bts.size(); i++) {
    if (!bts[i]->Release())
      backtraces_.erase(backtraces_.find(*bts[i]));
  }
}

}  // namespace profiling
