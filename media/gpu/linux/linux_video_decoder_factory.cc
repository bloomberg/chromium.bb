// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/linux_video_decoder_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/linux/mailbox_video_frame_converter.h"
#include "media/gpu/linux/platform_video_frame_pool.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"
#endif

namespace media {

namespace {

std::unique_ptr<VideoDecoder> CreateLinuxVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter) {
  if (!client_task_runner || !frame_pool || !frame_converter)
    return nullptr;

#if BUILDFLAG(USE_V4L2_CODEC)
  return V4L2SliceVideoDecoder::Create(std::move(client_task_runner),
                                       std::move(frame_pool),
                                       std::move(frame_converter));
#endif

  return nullptr;
}

}  // namespace

// static
std::unique_ptr<VideoDecoder> LinuxVideoDecoderFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb) {
  std::unique_ptr<DmabufVideoFramePool> frame_pool =
      std::make_unique<PlatformVideoFramePool>();
  std::unique_ptr<VideoFrameConverter> frame_converter =
      std::make_unique<MailboxVideoFrameConverter>(
          base::BindRepeating(&DmabufVideoFramePool::UnwrapFrame,
                              base::Unretained(frame_pool.get())),
          std::move(gpu_task_runner), std::move(get_stub_cb));

  return CreateLinuxVideoDecoder(std::move(client_task_runner),
                                 std::move(frame_pool),
                                 std::move(frame_converter));
}

// static
std::unique_ptr<VideoDecoder> LinuxVideoDecoderFactory::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  // Use VideoFrameConverter because we don't convert the frame to
  // Mailbox-backed.
  return CreateLinuxVideoDecoder(std::move(client_task_runner),
                                 std::make_unique<PlatformVideoFramePool>(),
                                 std::make_unique<VideoFrameConverter>());
}

}  // namespace media
