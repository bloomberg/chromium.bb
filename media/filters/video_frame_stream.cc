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
      decoder_selector_(new VideoDecoderSelector(
          message_loop, decoders.Pass(), set_decryptor_ready_cb)) {
}

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
  decoder_selector_->SelectVideoDecoder(this, statistics_cb, base::Bind(
      &VideoFrameStream::OnDecoderSelected, weak_this_));
}

void VideoFrameStream::ReadFrame(const VideoDecoder::ReadCB& read_cb) {
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
        read_cb, VideoDecoder::kDecodeError, scoped_refptr<VideoFrame>()));
    return;
  }

  read_cb_ = read_cb;
  decoder_->Read(base::Bind(&VideoFrameStream::OnFrameReady, weak_this_));
}

// VideoDecoder API guarantees that if VideoDecoder::Reset() is called during
// a pending read, the read callback must be fired before the reset callback is
// fired. Therefore, we can call VideoDecoder::Reset() regardless of if we have
// a pending read and always satisfy the reset callback when the decoder reset
// is finished. The only exception is when Reset() is called during decoder
// reinitialization. In this case we cannot and don't need to reset the decoder.
// We should just wait for the reinitialization to finish to satisfy the reset
// callback.
void VideoFrameStream::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_STOPPED) << state_;
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  reset_cb_ = closure;

  // VideoDecoder does not need to be and cannot be Reset() during
  // reinitialization. |decrypting_demuxer_stream_| was reset before decoder
  // reinitialization.
  if (state_ == STATE_REINITIALIZING_DECODER)
    return;

  // We may or may not have pending read, but we'll start to reset everything
  // regardless.
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

  // The stopping will continue after all of the following pending callbacks
  // (if they are not null) are satisfied.
  // TODO(xhwang): Now we cannot stop the initialization process through
  // VideoDecoderSelector. Fix this. See: http://crbug.com/222054
  if (state_ == STATE_INITIALIZING)
    return;

  // We may or may not have pending read and/or pending reset, but we'll start
  // to stop everything regardless.

  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &VideoFrameStream::StopDecoder, weak_this_));
    return;
  }

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

bool VideoFrameStream::HasOutputFrameAvailable() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return decoder_->HasOutputFrameAvailable();
}

void VideoFrameStream::Read(const DemuxerStream::ReadCB& demuxer_read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == STATE_FLUSHING_DECODER) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        demuxer_read_cb, DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer()));
    return;
  }

  stream_->Read(base::Bind(
      &VideoFrameStream::OnBufferReady, weak_this_, demuxer_read_cb));
}

AudioDecoderConfig VideoFrameStream::audio_decoder_config() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  LOG(FATAL) << "Method audio_decoder_config() called on VideoFrameStream";
  return stream_->audio_decoder_config();
}

VideoDecoderConfig VideoFrameStream::video_decoder_config() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return stream_->video_decoder_config();
}

DemuxerStream::Type VideoFrameStream::type() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return VIDEO;
}

void VideoFrameStream::EnableBitstreamConverter() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  NOTREACHED();
}

void VideoFrameStream::OnDecoderSelected(
    scoped_ptr<VideoDecoder> selected_decoder,
    scoped_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_INITIALIZING) << state_;
  DCHECK(!init_cb_.is_null());
  decoder_selector_.reset();

  if (!selected_decoder) {
    state_ = STATE_UNINITIALIZED;
    base::ResetAndReturn(&init_cb_).Run(false, false);
  } else {
    state_ = STATE_NORMAL;
    decoder_ = selected_decoder.Pass();
    decrypting_demuxer_stream_ = decrypting_demuxer_stream.Pass();
    if (decoder_->NeedsBitstreamConversion()) {
      stream_->EnableBitstreamConverter();
    }
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

void VideoFrameStream::OnFrameReady(const VideoDecoder::Status status,
                                    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER) << state_;
  DCHECK(!read_cb_.is_null());

  if (status != VideoDecoder::kOk) {
    DCHECK(!frame.get());
    state_ = STATE_ERROR;
    base::ResetAndReturn(&read_cb_).Run(status, NULL);
    return;
  }

  // The stopping/resetting process will be handled when the decoder is
  // stopped/reset.
  if (!stop_cb_.is_null() || !reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(VideoDecoder::kOk, NULL);
    return;
  }

  // Decoder flush finished. Reinitialize the video decoder.
  if (state_ == STATE_FLUSHING_DECODER &&
      status == VideoDecoder::kOk && frame->IsEndOfStream()) {
    ReinitializeDecoder();
    return;
  }

  base::ResetAndReturn(&read_cb_).Run(status, frame);
}

void VideoFrameStream::OnBufferReady(
    const DemuxerStream::ReadCB& demuxer_read_cb,
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  // VideoFrameStream reads from demuxer stream only when in NORMAL state.
  DCHECK_EQ(state_, STATE_NORMAL) << state_;
  DCHECK_EQ(buffer.get() != NULL, status == DemuxerStream::kOk) << status;

  if (status == DemuxerStream::kConfigChanged) {
    DVLOG(2) << "OnBufferReady() - kConfigChanged";
    state_ = STATE_FLUSHING_DECODER;
    demuxer_read_cb.Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  DCHECK(status == DemuxerStream::kOk || status == DemuxerStream::kAborted);
  demuxer_read_cb.Run(status, buffer);
}

void VideoFrameStream::ReinitializeDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING_DECODER) << state_;

  DemuxerStream* stream = this;
  if (decrypting_demuxer_stream_) {
    // TODO(xhwang): Remove this hack! Since VideoFrameStream handles
    // kConfigChange internally and hides it from downstream filters. The
    // DecryptingDemuxerStream never receives kConfigChanged to reset it's
    // internal VideoDecoderConfig. Call InitializeDecoderConfig() here
    // explicitly to solve this. This will be removed when we separate the
    // DemuxerStream from the VideoDecoder.
    decrypting_demuxer_stream_->InitializeDecoderConfig();
    stream = decrypting_demuxer_stream_.get();
  }

  DCHECK(stream->video_decoder_config().IsValidConfig());
  state_ = STATE_REINITIALIZING_DECODER;
  decoder_->Initialize(
      stream,
      base::Bind(&VideoFrameStream::OnDecoderReinitialized, weak_this_),
      statistics_cb_);
}

void VideoFrameStream::OnDecoderReinitialized(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER) << state_;

  state_ = (status == PIPELINE_OK) ? STATE_NORMAL : STATE_ERROR;

  if (!reset_cb_.is_null()) {
    if (!read_cb_.is_null())
      base::ResetAndReturn(&read_cb_).Run(VideoDecoder::kOk, NULL);
    base::ResetAndReturn(&reset_cb_).Run();
    return;
  }

  DCHECK(!read_cb_.is_null());

  if (!stop_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(VideoDecoder::kOk, NULL);
    return;
  }

  if (state_ == STATE_ERROR) {
    base::ResetAndReturn(&read_cb_).Run(VideoDecoder::kDecodeError, NULL);
    return;
  }

  decoder_->Read(base::Bind(&VideoFrameStream::OnFrameReady, weak_this_));
}

void VideoFrameStream::ResetDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL ||
         state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR) << state_;
  DCHECK(!reset_cb_.is_null());

  decoder_->Reset(base::Bind(&VideoFrameStream::OnDecoderReset, weak_this_));
}

void VideoFrameStream::OnDecoderReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL ||
         state_ == STATE_FLUSHING_DECODER ||
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
