// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

namespace base {

scoped_ptr<StackSamplingProfiler::NativeStackSampler>
StackSamplingProfiler::NativeStackSampler::Create(PlatformThreadId thread_id) {
  return scoped_ptr<NativeStackSampler>();
}

}  // namespace base
