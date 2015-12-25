// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_FAKE_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_FAKE_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <queue>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gl/gl_context.h"

namespace content {

class CONTENT_EXPORT FakeVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator {
 public:
  FakeVideoDecodeAccelerator(
      gfx::GLContext* gl,
      gfx::Size size,
      const base::Callback<bool(void)>& make_context_current);
  ~FakeVideoDecodeAccelerator() override;

  bool Initialize(const Config& config, Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool CanDecodeOnIOThread() override;

 private:
  void DoPictureReady();

  // The message loop that created the class. Used for all callbacks. This
  // class expects all calls to this class to be on this message loop (not
  // checked).
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  Client* client_;

  // Make our context current before running any GL entry points.
  base::Callback<bool(void)> make_context_current_;
  gfx::GLContext* gl_;

  // Output picture size.
  gfx::Size frame_buffer_size_;

  // Picture buffer ids that are available for putting fake frames in.
  std::queue<int> free_output_buffers_;
  // BitstreamBuffer ids for buffers that contain new data to decode.
  std::queue<int> queued_bitstream_ids_;

  bool flushing_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<FakeVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_FAKE_VIDEO_DECODE_ACCELERATOR_H_
