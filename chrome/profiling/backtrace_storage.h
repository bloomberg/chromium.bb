// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_BACKTRACE_STORAGE_H_
#define CHROME_PROFILING_BACKTRACE_STORAGE_H_

#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/profiling/backtrace.h"

namespace profiling {

// Backtraces are stored effectively as atoms, and this class is the backing
// store for the atoms. When you insert a backtrace, it will get de-duped with
// existing ones, one refcount added, and returned. When you're done with a
// backtrace, call Free() which will release the refcount. This may or may not
// release the underlying Backtrace itself, depending on whether other refs are
// held.
//
// This class is threadsafe.
class BacktraceStorage {
 public:
  // Instantiating this lock will prevent backtraces from being deleted from
  // the strorage for as long as it's alive. This class is moveable but not
  // copyable.
  class Lock {
   public:
    Lock();                                    // Doesn't take the lock.
    explicit Lock(BacktraceStorage* storage);  // Takes the lock.
    Lock(const Lock&) = delete;
    Lock(Lock&&);
    ~Lock();

    Lock& operator=(Lock&& other);
    Lock& operator=(const Lock&) = delete;

   private:
    BacktraceStorage* storage_;  // May be null if moved from.
  };

  BacktraceStorage();
  ~BacktraceStorage();

  // Adds the given backtrace to the storage and returns a key to it. If a
  // matching backtrace already exists, a key to the existing one will be
  // returned.
  //
  // The returned key will have a reference count associated with it, call
  // Free when the key is no longer needed.
  const Backtrace* Insert(std::vector<Address>&& bt);

  // Frees one reference to a backtrace.
  void Free(const Backtrace* bt);
  void Free(const std::vector<const Backtrace*>& bts);

 private:
  friend Lock;
  using Container = std::unordered_set<Backtrace>;

  // Called by the BacktraceStorage::Lock class.
  void LockStorage();
  void UnlockStorage();

  // Releases all backtraces in the vector assuming the lock_ is already held
  // and storage is not locked.
  void ReleaseBacktracesLocked(const std::vector<const Backtrace*>& bts);

  mutable base::Lock lock_;

  // Protected by the lock_. This indicates the number of consumers that have
  // raw backtrace pointers owned by backtraces_. As long as this count is
  // non-zero, Backtraces owned by backtraces_ cannot be modified or destroyed.
  int storage_lock_count_ = 0;

  // List of live backtraces for de-duping. Protected by the lock_.
  Container backtraces_;

  // When the backtrace storage is locked, no backtraces will be deleted from
  // the storage. Instead, they are accumulated here for releasing after the
  // lock is released.
  std::vector<const Backtrace*> release_after_lock_;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_BACKTRACE_STORAGE_H_
