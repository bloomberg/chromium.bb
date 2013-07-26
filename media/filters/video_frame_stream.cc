// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/video_decoder_selector.h"

namespace media {

VideoFrameStream::VideoFrameStream(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ScopedVector<VideoDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      state_(STATE_UNINITIALIZED),
      stream_(NULL),
      decoder_selector_(new VideoDecoderSelector(message_loop,
                                                 decoders.Pass(),
                                                 set_decryptor_ready_cb)) {}

VideoFrameStream::~VideoFrameStream() {
  DCHECK(state_ == STATE_UNINITIALIZED || state_ == STATE_STOPPED) << state_;
}

void VideoFrameStream::Initialize(DemuxerStream* stream,
                                  const StatisticsCB& statistics_cb,
                                  const InitCB& init_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED) << state_;
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());

  weak_this_ = weak_factory_.GetWeakPtr();

  statistics_cb_ = statistics_cb;
  init_cb_ = init_cb;
  stream_ = stream;

  state_ = STATE_INITIALIZING;
  // TODO(xhwang): VideoDecoderSelector only needs a config to select a decoder.
  decoder_selector_->SelectVideoDecoder(
      stream, base::Bind(&VideoFrameStream::OnDecoderSelected, weak_this_));
}

void VideoFrameStream::Read(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR) << state_;
  // No two reads in the flight at any time.
  DCHECK(read_cb_.is_null());
  // No read during resetting or stopping process.
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  if (state_ == STATE_ERROR) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        read_cb, DECODE_ERROR, scoped_refptr<VideoFrame>()));
    return;
  }

  read_cb_ = read_cb;

  if (state_ == STATE_FLUSHING_DECODER) {
    FlushDecoder();
    return;
  }

  ReadFromDemuxerStream();
}

void VideoFrameStream::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_STOPPED) << state_;
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  reset_cb_ = closure;

  // During decoder reinitialization, VideoDecoder does not need to be and
  // cannot be Reset(). |decrypting_demuxer_stream_| was reset before decoder
  // reinitialization.
  // During pending demuxer read, VideoDecoder will be reset after demuxer read
  // is returned (in OnBufferReady()).
  if (state_ == STATE_REINITIALIZING_DECODER ||
      state_ == STATE_PENDING_DEMUXER_READ) {
    return;
  }

  // VideoDecoder API guarantees that if VideoDecoder::Reset() is called during
  // a pending decode, the decode callback must be fired before the reset
  // callback is fired. Therefore, we can call VideoDecoder::Reset() regardless
  // of if we have a pending decode and always satisfy the reset callback when
  // the decoder reset is finished.
  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &VideoFrameStream::ResetDecoder, weak_this_));
    return;
  }

  ResetDecoder();
}

void VideoFrameStream::Stop(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, STATE_STOPPED) << state_;
  DCHECK(stop_cb_.is_null());

  stop_cb_ = closure;

  // The stopping process will continue after the pending operation is finished.
  // TODO(xhwang): Now we cannot stop the initialization process through
  // VideoDecoderSelector. Fix this. See: http://crbug.com/222054
  if (state_ == STATE_INITIALIZING || state_ == STATE_PENDING_DEMUXER_READ)
    return;

  // VideoDecoder API guarantees that if VideoDecoder::Stop() is called during
  // a pending reset or a pending decode, the callbacks are always fired in the
  // decode -> reset -> stop order. Therefore, we can call VideoDecoder::Stop()
  // regardless of if we have a pending decode or reset and always satisfy the
  // stop callback when the decoder decode/reset is finished.
  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &VideoFrameStream::StopDecoder, weak_this_));
    return;
  }

  // We may not have a |decoder_| if Stop() was called during initialization.
  if (decoder_) {
    StopDecoder();
    return;
  }

  state_ = STATE_STOPPED;
  stream_ = NULL;
  decoder_.reset();
  decrypting_demuxer_stream_.reset();
  message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&stop_cb_));
}

bool VideoFrameStream::CanReadWithoutStalling() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return decoder_->CanReadWithoutStalling();
}

void VideoFrameStream::OnDecoderSelected(
    scoped_ptr<VideoDecoder> selected_decoder,
    scoped_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  DCHECK(reset_cb_.is_null());

  decoder_selector_.reset();

  if (!selected_decoder) {
    state_ = STATE_UNINITIALIZED;
    base::ResetAndReturn(&init_cb_).Run(false, false);
  } else {
    state_ = STATE_NORMAL;
    decrypting_demuxer_stream_ = decrypting_demuxer_stream.Pass();
    if (decrypting_demuxer_stream_)
      stream_ = decrypting_demuxer_stream_.get();
    decoder_ = selected_decoder.Pass();
    if (decoder_->NeedsBitstreamConversion())
      stream_->EnableBitstreamConverter();
    // TODO(xhwang): We assume |decoder_->HasAlpha()| does not change after
    // reinitialization. Check this condition.
    base::ResetAndReturn(&init_cb_).Run(true, decoder_->HasAlpha());
  }

  // Stop() called during initialization.
  if (!stop_cb_.is_null()) {
    Stop(base::ResetAndReturn(&stop_cb_));
    return;
  }
}

void VideoFrameStream::SatisfyRead(Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(status, frame);
}

void VideoFrameStream::AbortRead() {
  SatisfyRead(ABORTED, NULL);
}

void VideoFrameStream::Decode(const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());
  DCHECK(buffer);

  int buffer_size = buffer->end_of_stream() ? 0 : buffer->data_size();
  decoder_->Decode(buffer, base::Bind(&VideoFrameStream::OnFrameReady,
                                      weak_this_, buffer_size));
}

void VideoFrameStream::FlushDecoder() {
  Decode(DecoderBuffer::CreateEOSBuffer());
}

void VideoFrameStream::OnFrameReady(int buffer_size,
                                    const VideoDecoder::Status status,
                                    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER) << state_;
  DCHECK(!read_cb_.is_null());

  if (status == VideoDecoder::kDecodeError) {
    DCHECK(!frame.get());
    state_ = STATE_ERROR;
    SatisfyRead(DECODE_ERROR, NULL);
    return;
  }

  if (status == VideoDecoder::kDecryptError) {
    DCHECK(!frame.get());
    state_ = STATE_ERROR;
    SatisfyRead(DECRYPT_ERROR, NULL);
    return;
  }

  // Any successful decode counts!
  if (buffer_size > 0) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer_size;
    statistics_cb_.Run(statistics);
  }

  // Drop decoding result if Reset()/Stop() was called during decoding.
  // The stopping/resetting process will be handled when the decoder is
  // stopped/reset.
  if (!stop_cb_.is_null() || !reset_cb_.is_null()) {
    AbortRead();
    return;
  }

  // Decoder flushed. Reinitialize the video decoder.
  if (state_ == STATE_FLUSHING_DECODER &&
      status == VideoDecoder::kOk && frame->IsEndOfStream()) {
    ReinitializeDecoder();
    return;
  }

  if (status == VideoDecoder::kNotEnoughData) {
    if (state_ == STATE_NORMAL)
      ReadFromDemuxerStream();
    else if (state_ == STATE_FLUSHING_DECODER)
      FlushDecoder();
    return;
  }

  SatisfyRead(OK, frame);
}

void VideoFrameStream::ReadFromDemuxerStream() {
  DCHECK_EQ(state_, STATE_NORMAL) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  state_ = STATE_PENDING_DEMUXER_READ;
  stream_->Read(base::Bind(&VideoFrameStream::OnBufferReady, weak_this_));
}

void VideoFrameStream::OnBufferReady(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_PENDING_DEMUXER_READ) << state_;
  DCHECK_EQ(buffer.get() != NULL, status == DemuxerStream::kOk) << status;
  DCHECK(!read_cb_.is_null());

  state_ = STATE_NORMAL;

  // Reset()/Stop() was postponed during STATE_PENDING_DEMUXER_READ state.
  // We need to handle them in this function.

  if (!stop_cb_.is_null()) {
    AbortRead();
    if (!reset_cb_.is_null())
      Reset(base::ResetAndReturn(&reset_cb_));
    Stop(base::ResetAndReturn(&stop_cb_));
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    state_ = STATE_FLUSHING_DECODER;
    if (!reset_cb_.is_null()) {
      AbortRead();
      Reset(base::ResetAndReturn(&reset_cb_));
      // Reinitialization will continue after Reset() is done.
    } else {
      FlushDecoder();
    }
    return;
  }

  if (!reset_cb_.is_null()) {
    AbortRead();
    Reset(base::ResetAndReturn(&reset_cb_));
    return;
  }

  if (status == DemuxerStream::kAborted) {
    SatisfyRead(DEMUXER_READ_ABORTED, NULL);
    return;
  }

  DCHECK(status == DemuxerStream::kOk) << status;
  Decode(buffer);
}

void VideoFrameStream::ReinitializeDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING_DECODER) << state_;

  DCHECK(stream_->video_decoder_config().IsValidConfig());
  state_ = STATE_REINITIALIZING_DECODER;
  decoder_->Initialize(
      stream_->video_decoder_config(),
      base::Bind(&VideoFrameStream::OnDecoderReinitialized, weak_this_));
}

void VideoFrameStream::OnDecoderReinitialized(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER) << state_;

  // ReinitializeDecoder() can be called in two cases:
  // 1, Flushing decoder finished (see OnFrameReady()).
  // 2, Reset() was called during flushing decoder (see OnDecoderReset()).
  // Also, Reset()/Stop() can be called during pending ReinitializeDecoder().
  // This function needs to handle them all!

  state_ = (status == PIPELINE_OK) ? STATE_NORMAL : STATE_ERROR;

  if (!read_cb_.is_null() && (!stop_cb_.is_null() || !reset_cb_.is_null()))
    AbortRead();

  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();

  // If !stop_cb_.is_null(), it will be handled in OnDecoderStopped().

  if (read_cb_.is_null())
    return;

  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(ABORTED, NULL);
    return;
  }

  if (state_ == STATE_ERROR) {
    SatisfyRead(DECODE_ERROR, NULL);
    return;
  }

  ReadFromDemuxerStream();
}

void VideoFrameStream::ResetDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR) << state_;
  DCHECK(!reset_cb_.is_null());

  decoder_->Reset(base::Bind(&VideoFrameStream::OnDecoderReset, weak_this_));
}

void VideoFrameStream::OnDecoderReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR) << state_;
  // If Reset() was called during pending read, read callback should be fired
  // before the reset callback is fired.
  DCHECK(read_cb_.is_null());
  DCHECK(!reset_cb_.is_null());

  if (state_ != STATE_FLUSHING_DECODER || !stop_cb_.is_null()) {
    base::ResetAndReturn(&reset_cb_).Run();
    return;
  }

  // The resetting process will be continued in OnDecoderReinitialized().
  ReinitializeDecoder();
}

void VideoFrameStream::StopDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_STOPPED) << state_;
  DCHECK(!stop_cb_.is_null());

  decoder_->Stop(base::Bind(&VideoFrameStream::OnDecoderStopped, weak_this_));
}

void VideoFrameStream::OnDecoderStopped() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_STOPPED) << state_;
  // If Stop() was called during pending read/reset, read/reset callback should
  // be fired before the stop callback is fired.
  DCHECK(read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(!stop_cb_.is_null());

  state_ = STATE_STOPPED;
  stream_ = NULL;
  decoder_.reset();
  decrypting_demuxer_stream_.reset();
  base::ResetAndReturn(&stop_cb_).Run();
}

}  // namespace media
