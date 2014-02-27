// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_gpu_video_accelerator_factories.h"

#include "base/logging.h"
#include "media/cast/test/fake_video_encode_accelerator.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
namespace cast {
namespace test {

FakeGpuVideoAcceleratorFactories::FakeGpuVideoAcceleratorFactories(
    const scoped_refptr<base::SingleThreadTaskRunner>& fake_task_runner)
    : fake_task_runner_(fake_task_runner) {}

FakeGpuVideoAcceleratorFactories::~FakeGpuVideoAcceleratorFactories() {}

scoped_ptr<VideoEncodeAccelerator>
FakeGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return scoped_ptr<VideoEncodeAccelerator>(new FakeVideoEncodeAccelerator());
}

base::SharedMemory* FakeGpuVideoAcceleratorFactories::CreateSharedMemory(
    size_t size) {
  base::SharedMemory* shm = new base::SharedMemory();
  if (!shm->CreateAndMapAnonymous(size)) {
    NOTREACHED();
  }
  return shm;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeGpuVideoAcceleratorFactories::GetTaskRunner() {
  return fake_task_runner_;
}

uint32 FakeGpuVideoAcceleratorFactories::CreateTextures(
    int32 count,
    const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32 texture_target) {
  return 0;
}

scoped_ptr<VideoDecodeAccelerator>
FakeGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator(
    VideoCodecProfile profile) {
  return scoped_ptr<VideoDecodeAccelerator>(
      static_cast<media::VideoDecodeAccelerator*>(NULL));
}

}  // namespace test
}  // namespace cast
}  // namespace media
