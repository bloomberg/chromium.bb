// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/single_release_callback_impl.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cc/trees/blocking_task_runner.h"

namespace cc {

SingleReleaseCallbackImpl::SingleReleaseCallbackImpl(
    const ReleaseCallbackImpl& callback)
    : has_been_run_(false), callback_(callback) {
  DCHECK(!callback_.is_null())
      << "Use a NULL SingleReleaseCallbackImpl for an empty callback.";
}

SingleReleaseCallbackImpl::~SingleReleaseCallbackImpl() {
  DCHECK(callback_.is_null() || has_been_run_)
      << "SingleReleaseCallbackImpl was never run.";
}

void SingleReleaseCallbackImpl::Run(
    uint32 sync_point,
    bool is_lost,
    BlockingTaskRunner* main_thread_task_runner) {
  DCHECK(!has_been_run_) << "SingleReleaseCallbackImpl was run more than once.";
  has_been_run_ = true;
  callback_.Run(sync_point, is_lost, main_thread_task_runner);
}

}  // namespace cc
