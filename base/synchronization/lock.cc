// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"

#include "base/logging.h"

namespace base {

#if !defined(NDEBUG)
const PlatformThreadId kNoThreadId = static_cast<PlatformThreadId>(0);
#endif

Lock::Lock() : lock_() {
#if !defined(NDEBUG)
  owned_by_thread_ = false;
  owning_thread_id_ = kNoThreadId;
#endif
}

Lock::~Lock() {
#if !defined(NDEBUG)
  DCHECK(!owned_by_thread_);
  DCHECK_EQ(kNoThreadId, owning_thread_id_);
#endif
}

void Lock::Acquire() {
  lock_.Lock();
#if !defined(NDEBUG)
  CheckUnheldAndMark();
#endif
}
void Lock::Release() {
#if !defined(NDEBUG)
  CheckHeldAndUnmark();
#endif
  lock_.Unlock();
}

bool Lock::Try() {
  bool rv = lock_.Try();
#if !defined(NDEBUG)
  if (rv) {
    CheckUnheldAndMark();
  }
#endif
  return rv;
}

#if !defined(NDEBUG)
void Lock::AssertAcquired() const {
  DCHECK(owned_by_thread_);
  DCHECK_EQ(owning_thread_id_, PlatformThread::CurrentId());
}

void Lock::CheckHeldAndUnmark() {
  DCHECK(owned_by_thread_);
  DCHECK_EQ(owning_thread_id_, PlatformThread::CurrentId());
  owned_by_thread_ = false;
  owning_thread_id_ = kNoThreadId;
}

void Lock::CheckUnheldAndMark() {
  DCHECK(!owned_by_thread_);
  owned_by_thread_ = true;
  owning_thread_id_ = PlatformThread::CurrentId();
}
#endif

}  // namespace base
