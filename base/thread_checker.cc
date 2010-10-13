// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_checker.h"

// This code is only done in debug builds.
#ifndef NDEBUG

ThreadChecker::ThreadChecker() {
  EnsureThreadIdAssigned();
}

ThreadChecker::~ThreadChecker() {}

bool ThreadChecker::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  return *valid_thread_id_ == PlatformThread::CurrentId();
}

void ThreadChecker::DetachFromThread() {
  valid_thread_id_.reset();
}

void ThreadChecker::EnsureThreadIdAssigned() const {
  if (valid_thread_id_.get())
    return;
  valid_thread_id_.reset(new PlatformThreadId(PlatformThread::CurrentId()));
}

#endif  // NDEBUG
