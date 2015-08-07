// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/frame_status_cb_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace chromecast {
namespace media {

FrameStatusCBImpl::FrameStatusCBImpl(const CallbackType& cb)
    : cb_(cb), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FrameStatusCBImpl::~FrameStatusCBImpl() {}

void FrameStatusCBImpl::Run(MediaComponentDevice::FrameStatus status) {
  if (task_runner_->BelongsToCurrentThread()) {
    if (!cb_.is_null())
      cb_.Run(status);
  } else {
    task_runner_->PostTask(FROM_HERE, base::Bind(cb_, status));
  }
}

}  // namespace media
}  // namespace chromecast
