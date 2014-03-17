// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "media/base/callback_holder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/size.h"

using base::ResetAndReturn;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class FakeVideoDecoder : public VideoDecoder {
 public:
  // Constructs an object with a decoding delay of |decoding_delay| frames.
  explicit FakeVideoDecoder(int decoding_delay,
                            bool supports_get_decode_output);
  virtual ~FakeVideoDecoder();

  // VideoDecoder implementation.
  virtual void Initialize(const VideoDecoderConfig& config,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual scoped_refptr<VideoFrame> GetDecodeOutput() OVERRIDE;

  // Holds the next init/read/reset/stop callback from firing.
  void HoldNextInit();
  void HoldNextRead();
  void HoldNextReset();
  void HoldNextStop();

  // Satisfies the pending init/read/reset/stop callback, which must be ready
  // to fire when these methods are called.
  void SatisfyInit();
  void SatisfyRead();
  void SatisfyReset();
  void SatisfyStop();

  int total_bytes_decoded() const { return total_bytes_decoded_; }

 private:
  enum State {
    UNINITIALIZED,
    NORMAL
  };

  // Callback for updating |total_bytes_decoded_|.
  void OnFrameDecoded(int buffer_size,
                      const DecodeCB& read_cb,
                      Status status,
                      const scoped_refptr<VideoFrame>& video_frame);

  void DoReset();
  void DoStop();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeVideoDecoder> weak_factory_;
  base::WeakPtr<FakeVideoDecoder> weak_this_;

  const int decoding_delay_;

  bool supports_get_decode_output_;

  State state_;

  CallbackHolder<PipelineStatusCB> init_cb_;
  CallbackHolder<DecodeCB> decode_cb_;
  CallbackHolder<base::Closure> reset_cb_;
  CallbackHolder<base::Closure> stop_cb_;

  VideoDecoderConfig current_config_;

  std::list<scoped_refptr<VideoFrame> > decoded_frames_;

  int total_bytes_decoded_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
