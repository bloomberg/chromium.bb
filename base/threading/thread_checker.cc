// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker.h"

// This code is only done in debug builds.
#ifndef NDEBUG

namespace base {

ThreadChecker::ThreadChecker() : valid_thread_id_(kInvalidThreadId) {
  EnsureThreadIdAssigned();
}

ThreadChecker::~ThreadChecker() {}

bool ThreadChecker::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  AutoLock auto_lock(lock_);
  return valid_thread_id_ == PlatformThread::CurrentId();
}

void ThreadChecker::DetachFromThread() {
  AutoLock auto_lock(lock_);
  valid_thread_id_ = kInvalidThreadId;
}

void ThreadChecker::EnsureThreadIdAssigned() const {
  AutoLock auto_lock(lock_);
  if (valid_thread_id_ != kInvalidThreadId)
    return;
  valid_thread_id_ = PlatformThread::CurrentId();
}

}  // namespace base

#endif  // NDEBUG
