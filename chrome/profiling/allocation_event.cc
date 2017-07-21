// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/allocation_event.h"

namespace profiling {

AllocationEvent::AllocationEvent(Address addr, size_t sz, const Backtrace* bt)
    : address_(addr), size_(sz), backtrace_(bt) {}

AllocationEvent::AllocationEvent(Address addr)
    : address_(addr), size_(0), backtrace_(nullptr) {}

}  // namespace profiling
