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

// This class is threadsafe.
class BacktraceStorage {
 public:
  using Container = std::unordered_set<Backtrace>;
  using Key = Container::iterator;

  BacktraceStorage();
  ~BacktraceStorage();

  // Adds the given backtrace to the storage and returns a key to it. If a
  // matching backtrace already exists, a key to the existing one will be
  // returned.
  //
  // The returned key will have a reference count associated with it, call
  // Free when the key is no longer needed.
  Key Insert(std::vector<Address>&& bt);

  // Frees one reference to a backtrace.
  void Free(const Key& key);
  void Free(const std::vector<Key>& keys);

  // Returns the backtrace associated with the given key. Assumes the caller
  // holds a key to it that will keep the backtrace in scope.
  const Backtrace& GetBacktraceForKey(const Key& key) const;

 private:
  mutable base::Lock lock_;

  // List of live backtraces for de-duping. Protected by the lock_.
  Container backtraces_;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_BACKTRACE_STORAGE_H_
