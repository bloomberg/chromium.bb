// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/video_pipeline_device_client_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/public/graphics_types.h"

namespace chromecast {
namespace media {

VideoPipelineDeviceClientImpl::VideoPipelineDeviceClientImpl(
    const SizeChangeCB& size_change_cb)
    : size_change_cb_(size_change_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

VideoPipelineDeviceClientImpl::~VideoPipelineDeviceClientImpl() {}

void VideoPipelineDeviceClientImpl::OnNaturalSizeChanged(const Size& size) {
  if (task_runner_->BelongsToCurrentThread()) {
    if (!size_change_cb_.is_null())
      size_change_cb_.Run(size);
  } else {
    task_runner_->PostTask(FROM_HERE, base::Bind(size_change_cb_, size));
  }
}

}  // namespace media
}  // namespace chromecast
