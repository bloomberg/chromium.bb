// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/backtrace_storage.h"

#include "chrome/profiling/backtrace.h"

namespace profiling {

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
  if (!bt->Release())
    backtraces_.erase(backtraces_.find(*bt));
}

void BacktraceStorage::Free(const std::vector<const Backtrace*>& bts) {
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < bts.size(); i++) {
    if (!bts[i]->Release())
      backtraces_.erase(backtraces_.find(*bts[i]));
  }
}

}  // namespace profiling
