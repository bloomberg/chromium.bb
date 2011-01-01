// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/non_thread_safe.h"

// These checks are only done in debug builds.
#ifndef NDEBUG

#include "base/logging.h"

namespace base {

NonThreadSafe::~NonThreadSafe() {
  DCHECK(CalledOnValidThread());
}

bool NonThreadSafe::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void NonThreadSafe::DetachFromThread() {
  thread_checker_.DetachFromThread();
}

}  // namespace base

#endif  // NDEBUG
