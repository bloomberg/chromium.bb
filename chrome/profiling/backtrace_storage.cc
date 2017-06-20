// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/backtrace_storage.h"

#include "chrome/profiling/backtrace.h"

namespace profiling {

BacktraceStorage::BacktraceStorage() {}

BacktraceStorage::~BacktraceStorage() {}

BacktraceStorage::Key BacktraceStorage::Insert(std::vector<Address>&& bt) {
  base::AutoLock lock(lock_);

  BacktraceStorage::Key key =
      backtraces_.insert(Backtrace(std::move(bt))).first;
  key->AddRef();
  return key;
}

void BacktraceStorage::Free(const Key& key) {
  base::AutoLock lock(lock_);
  if (!key->Release())
    backtraces_.erase(key);
}

void BacktraceStorage::Free(const std::vector<Key>& keys) {
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < keys.size(); i++) {
    if (!keys[i]->Release())
      backtraces_.erase(keys[i]);
  }
}

const Backtrace& BacktraceStorage::GetBacktraceForKey(const Key& key) const {
  // Since the caller should own a reference to the key and the container has
  // stable iterators, we can access without a lock.
  return *key;
}

}  // namespace profiling
