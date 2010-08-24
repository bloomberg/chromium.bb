// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_checker.h"

// This code is only done in debug builds.
#ifndef NDEBUG
ThreadChecker::ThreadChecker() : valid_thread_id_(PlatformThread::CurrentId()) {
}

bool ThreadChecker::CalledOnValidThread() const {
  return valid_thread_id_ == PlatformThread::CurrentId();
}

#endif  // NDEBUG
