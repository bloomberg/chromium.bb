// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_MEMORY_BUFFER_DECODER_WRAPPER_H_
#define MEDIA_FILTERS_GPU_MEMORY_BUFFER_DECODER_WRAPPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder.h"

namespace media {
class GpuMemoryBufferVideoFramePool;

// Wrapper for VideoDecoder implementations that copies the frames returned by
// |output_cb| into GpuMemoryBuffers.
//
// Subsequent pipeline stages (e.g., VideoFrameStream, VideoRendererImpl) may
// each have a private cache. Some time may pass before frames move from one
// cache to the next depending on current buffering levels. If the last stage is
// to copy frames to GpuMemoryBuffers, we waste the idle time between caches.
//
// As such, it's most efficient to copy frames immediately as they come out of
// the decoder instead of later to ensure they are copied expediently.
class MEDIA_EXPORT GpuMemoryBufferDecoderWrapper : public VideoDecoder {
 public:
  GpuMemoryBufferDecoderWrapper(
      std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool,
      std::unique_ptr<VideoDecoder> decoder);
  ~GpuMemoryBufferDecoderWrapper() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;  // Returns |decoder_| name.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& reset_cb) override;
  int GetMaxDecodeRequests() const override;  // Returns |decoder_| value.

 private:
  void OnDecodedEOS(const DecodeCB& decode_cb, DecodeStatus status);
  void OnOutputReady(const OutputCB& output_cb,
                     const scoped_refptr<VideoFrame>& frame);
  void OnFrameCopied(const OutputCB& output_cb,
                     const scoped_refptr<VideoFrame>& frame);

  THREAD_CHECKER(thread_checker_);

  // Pool of GpuMemoryBuffers and resources used to create hardware frames.
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool_;

  // The decoder which will be wrapped.
  std::unique_ptr<VideoDecoder> decoder_;

  // Set upon Reset(), cleared by Decode(). Allows us to abort copies for better
  // seeking performance.
  bool abort_copies_ = false;

  // These two variables are used to track and trigger the final end of stream
  // notification once all copies are complete. Cleared by Reset(), set by
  // Decode() when an end of stream buffer is received.
  uint32_t pending_copies_ = 0u;
  base::OnceClosure eos_decode_cb_;

  base::WeakPtrFactory<GpuMemoryBufferDecoderWrapper> weak_factory_;

  // Separate WeakPtr factory which is used to abort copies after a Reset() so
  // they don't mess up the |pending_copies_| count.
  base::WeakPtrFactory<GpuMemoryBufferDecoderWrapper> copy_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferDecoderWrapper);
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_MEMORY_BUFFER_DECODER_WRAPPER_H_
