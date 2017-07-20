// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/allocation_event.h"

namespace profiling {

AllocationEvent::AllocationEvent(Address addr,
                                 size_t sz,
                                 BacktraceStorage::Key key)
    : address_(addr), size_(sz), backtrace_key_(key) {}

AllocationEvent::AllocationEvent(Address addr)
    : address_(addr), size_(0), backtrace_key_() {}

}  // namespace profiling
