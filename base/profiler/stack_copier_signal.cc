// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_copier_signal.h"

#include "base/profiler/metadata_recorder.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/thread_delegate.h"

namespace base {

StackCopierSignal::StackCopierSignal() = default;

StackCopierSignal::~StackCopierSignal() = default;

bool StackCopierSignal::CopyStack(StackBuffer* stack_buffer,
                                  uintptr_t* stack_top,
                                  ProfileBuilder* profile_builder,
                                  RegisterContext* thread_context) {
  // TODO(wittman): Implement signal-based stack copying.
  return false;
}

}  // namespace base
