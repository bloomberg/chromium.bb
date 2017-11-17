// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_
#define MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {

class MEDIA_EXPORT OffloadableVideoDecoder : public VideoDecoder {
 public:
  ~OffloadableVideoDecoder() override {}

  // Called by the OffloadingVideoDecoder when closing the decoder and switching
  // task runners. Will be called on the task runner that Initialize() was
  // called on. Upon completion of this method implementing decoders must be
  // ready to be Initialized() on another thread.
  virtual void Detach() = 0;
};

// Wrapper for OffloadableVideoDecoder implementations that runs the wrapped
// decoder on a task pool other than the caller's thread.
//
// Optionally decoders which are aware of the wrapping may choose to not rebind
// callbacks to the offloaded thread since they will already be bound by the
// OffloadingVideoDecoder; this simply avoids extra hops for completed tasks.
class MEDIA_EXPORT OffloadingVideoDecoder : public VideoDecoder {
 public:
  // Offloads |decoder| for VideoDecoderConfigs provided to Initialize() using
  // |supported_codecs| with a coded width >= |min_offloading_width|.
  //
  // E.g. if a width of 1024 is specified, and VideoDecoderConfig has a coded
  // size of 1280x720 we will use offloading. Conversely if the width was
  // 640x480, we would not use offloading.
  OffloadingVideoDecoder(int min_offloading_width,
                         std::vector<VideoCodec> supported_codecs,
                         std::unique_ptr<OffloadableVideoDecoder> decoder);
  ~OffloadingVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& reset_cb) override;

 private:
  // VideoDecoderConfigs given to Initialize() with a coded size that has width
  // greater than or equal to this value will be offloaded.
  const int min_offloading_width_;

  // Codecs supported for offloading.
  const std::vector<VideoCodec> supported_codecs_;

  // Indicates if Initialize() has been called.
  bool initialized_ = false;

  THREAD_CHECKER(thread_checker_);

  // The decoder which will be offloaded.
  std::unique_ptr<OffloadableVideoDecoder> decoder_;

  // High resolution decodes may block the media thread for too long, in such
  // cases offload the decoding to a task pool.
  scoped_refptr<base::SequencedTaskRunner> offload_task_runner_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<OffloadingVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OffloadingVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OFFLOADING_VIDEO_DECODER_H_
