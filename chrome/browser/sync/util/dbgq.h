// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_DBGQ_H_
#define CHROME_BROWSER_SYNC_UTIL_DBGQ_H_
#pragma once

#include "base/basictypes.h"  // for COMPILE_ASSERT

// A circular queue that is designed to be easily inspectable in a debugger. It
// puts the elements into the array in reverse, so you can just look at the i_
// pointer for a recent history.
template <typename T, size_t size>
class DebugQueue {
  COMPILE_ASSERT(size > 0, DebugQueue_size_must_be_greater_than_zero);
 public:
  DebugQueue() : i_(array_) { }
  void Push(const T& t) {
    i_ = (array_ == i_ ? array_ + size - 1 : i_ - 1);
    *i_ = t;
  }
 protected:
  T* i_;  // Points to the newest element in the queue.
  T array_[size];
};

#endif  // CHROME_BROWSER_SYNC_UTIL_DBGQ_H_
