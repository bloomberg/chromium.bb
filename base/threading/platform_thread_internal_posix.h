// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
#define BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_

#include "base/threading/platform_thread.h"

namespace base {

namespace internal {

struct ThreadPriorityToNiceValuePair {
  ThreadPriority priority;
  int nice_value;
};
extern const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4];

// Returns the nice value matching |priority| based on the platform-specific
// implementation of kThreadPriorityToNiceValueMap.
int ThreadPriorityToNiceValue(ThreadPriority priority);

// Allows platform specific tweaks to the generic POSIX solution for
// SetThreadPriority. Returns true if the platform-specific implementation
// handled this |priority| change, false if the generic implementation should
// instead proceed.
bool HandleSetThreadPriorityForPlatform(PlatformThreadHandle handle,
                                        ThreadPriority priority);

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
