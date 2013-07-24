// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MOCK_GPU_VIDEO_DECODER_FACTORIES_H_
#define MEDIA_FILTERS_MOCK_GPU_VIDEO_DECODER_FACTORIES_H_

#include "base/message_loop/message_loop_proxy.h"
#include "media/filters/gpu_video_decoder_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"

template <class T>
class scoped_refptr;

namespace base {
class SharedMemory;
}

namespace media {

class MockGpuVideoDecoderFactories : public GpuVideoDecoderFactories {
 public:
  MockGpuVideoDecoderFactories();
  MOCK_METHOD2(CreateVideoDecodeAccelerator,
               VideoDecodeAccelerator*(VideoCodecProfile,
                                       VideoDecodeAccelerator::Client*));
  MOCK_METHOD5(CreateTextures,
               uint32(int32 count,
                      const gfx::Size& size,
                      std::vector<uint32>* texture_ids,
                      std::vector<gpu::Mailbox>* texture_mailboxes,
                      uint32 texture_target));
  MOCK_METHOD1(DeleteTexture, void(uint32 texture_id));
  MOCK_METHOD1(WaitSyncPoint, void(uint32 sync_point));
  MOCK_METHOD4(ReadPixels,
               void(uint32 texture_id,
                    uint32 texture_target,
                    const gfx::Size& size,
                    const SkBitmap& pixels));
  MOCK_METHOD1(CreateSharedMemory, base::SharedMemory*(size_t size));
  MOCK_METHOD0(GetMessageLoop, scoped_refptr<base::MessageLoopProxy>());
  MOCK_METHOD0(Abort, void());
  MOCK_METHOD0(IsAborted, bool());

 private:
  virtual ~MockGpuVideoDecoderFactories();

  DISALLOW_COPY_AND_ASSIGN(MockGpuVideoDecoderFactories);
};

}  // namespace media

#endif  // MEDIA_FILTERS_MOCK_GPU_VIDEO_DECODER_FACTORIES_H_
