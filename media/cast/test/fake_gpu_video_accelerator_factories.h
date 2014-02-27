// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_FAKE_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_CAST_TEST_FAKE_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include "media/filters/gpu_video_accelerator_factories.h"

#include "base/message_loop/message_loop.h"
#include "media/cast/test/fake_single_thread_task_runner.h"

namespace media {
namespace cast {
namespace test {

class FakeGpuVideoAcceleratorFactories : public GpuVideoAcceleratorFactories {
 public:
  explicit FakeGpuVideoAcceleratorFactories(
      const scoped_refptr<base::SingleThreadTaskRunner>& fake_task_runner);

  virtual scoped_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      OVERRIDE;

  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetTaskRunner() OVERRIDE;
  //
  //  The following functions are no-op.
  //
  virtual uint32 CreateTextures(int32 count,
                                const gfx::Size& size,
                                std::vector<uint32>* texture_ids,
                                std::vector<gpu::Mailbox>* texture_mailboxes,
                                uint32 texture_target) OVERRIDE;

  virtual void DeleteTexture(uint32 texture_id) OVERRIDE {}

  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE {}

  virtual void ReadPixels(uint32 texture_id,
                          const gfx::Rect& visible_rect,
                          const SkBitmap& pixels) OVERRIDE {};

  virtual scoped_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator(
      VideoCodecProfile profile) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<FakeGpuVideoAcceleratorFactories>;
  virtual ~FakeGpuVideoAcceleratorFactories();

  const scoped_refptr<base::SingleThreadTaskRunner> fake_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FakeGpuVideoAcceleratorFactories);
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_FAKE_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
