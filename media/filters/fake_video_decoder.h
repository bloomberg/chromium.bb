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
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/size.h"

using base::ResetAndReturn;

namespace base {
class MessageLoopProxy;
}

namespace media {

class MEDIA_EXPORT FakeVideoDecoder : public VideoDecoder {
 public:
  // Constructs an object with a decoding delay of |decoding_delay| frames.
  explicit FakeVideoDecoder(int decoding_delay);
  virtual ~FakeVideoDecoder();

  // VideoDecoder implementation.
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

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

 private:
  enum State {
    UNINITIALIZED,
    NORMAL
  };

  void ReadFromDemuxerStream();

  // Callback for DemuxerStream::Read().
  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer);

  void DoReset();
  void DoStop();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<FakeVideoDecoder> weak_factory_;
  base::WeakPtr<FakeVideoDecoder> weak_this_;

  const int decoding_delay_;

  State state_;

  StatisticsCB statistics_cb_;

  CallbackHolder<PipelineStatusCB> init_cb_;
  CallbackHolder<ReadCB> read_cb_;
  CallbackHolder<base::Closure> reset_cb_;
  CallbackHolder<base::Closure> stop_cb_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  DemuxerStream* demuxer_stream_;

  VideoDecoderConfig current_config_;

  std::list<scoped_refptr<VideoFrame> > decoded_frames_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
