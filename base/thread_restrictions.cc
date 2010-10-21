// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_restrictions.h"

// This entire file is compiled out in Release mode.
#ifndef NDEBUG

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/thread_local.h"

namespace base {

namespace {

LazyInstance<ThreadLocalBoolean, LeakyLazyInstanceTraits<ThreadLocalBoolean> >
    g_io_disallowed(LINKER_INITIALIZED);

}  // anonymous namespace

// static
void ThreadRestrictions::SetIOAllowed(bool allowed) {
  g_io_disallowed.Get().Set(!allowed);
}

// static
void ThreadRestrictions::AssertIOAllowed() {
  if (g_io_disallowed.Get().Get()) {
    LOG(FATAL) <<
        "Function marked as IO-only was called from a thread that "
        "disallows IO!  If this thread really should be allowed to "
        "make IO calls, adjust the call to "
        "base::ThreadRestrictions::SetIOAllowed() in this thread's "
        "startup.";
  }
}

}  // namespace base

#endif  // NDEBUG
