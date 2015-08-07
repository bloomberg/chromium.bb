// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_component_device_client_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace chromecast {
namespace media {

MediaComponentDeviceClientImpl::MediaComponentDeviceClientImpl(
    const base::Closure& eos_cb)
    : eos_cb_(eos_cb), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

MediaComponentDeviceClientImpl::~MediaComponentDeviceClientImpl() {}

void MediaComponentDeviceClientImpl::OnEndOfStream() {
  if (task_runner_->BelongsToCurrentThread()) {
    if (!eos_cb_.is_null())
      eos_cb_.Run();
  } else {
    task_runner_->PostTask(FROM_HERE, eos_cb_);
  }
}

}  // namespace media
}  // namespace chromecast
