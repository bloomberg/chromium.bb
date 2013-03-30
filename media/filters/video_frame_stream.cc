// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/video_decoder_selector.h"

namespace media {

VideoFrameStream::VideoFrameStream(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      state_(UNINITIALIZED),
      set_decryptor_ready_cb_(set_decryptor_ready_cb) {
}

VideoFrameStream::~VideoFrameStream() {
  DCHECK(state_ == UNINITIALIZED || state_ == STOPPED) << state_;
}

void VideoFrameStream::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                  const VideoDecoderList& decoders,
                                  const StatisticsCB& statistics_cb,
                                  const InitCB& init_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, UNINITIALIZED);

  weak_this_ = weak_factory_.GetWeakPtr();

  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  init_cb_ = init_cb;
  stream_ = stream;

  scoped_ptr<VideoDecoderSelector> decoder_selector(
      new VideoDecoderSelector(message_loop_,
                               decoders,
                               set_decryptor_ready_cb_));

  // To avoid calling |decoder_selector| methods and passing ownership of
  // |decoder_selector| in the same line.
  VideoDecoderSelector* decoder_selector_ptr = decoder_selector.get();

  decoder_selector_ptr->SelectVideoDecoder(
      this,
      statistics_cb,
      base::Bind(&VideoFrameStream::OnDecoderSelected, weak_this_,
                 base::Passed(&decoder_selector)));
}

void VideoFrameStream::ReadFrame(const VideoDecoder::ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  // No two reads in the flight at any time.
  DCHECK(read_cb_.is_null());
  // No read during resetting or stopping process.
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  read_cb_ = read_cb;

  decoder_->Read(base::Bind(&VideoFrameStream::OnFrameRead, weak_this_));
}

void VideoFrameStream::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  DCHECK(reset_cb_.is_null());
  DCHECK(stop_cb_.is_null());

  reset_cb_ = closure;

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
  DCHECK_NE(state_, STOPPED);
  DCHECK(stop_cb_.is_null());

  stop_cb_ = closure;

  // The stopping will continue after all of the following pending callbacks
  // (if they are not null) are satisfied.
  // TODO(xhwang): Now we cannot stop the initialization process through
  // VideoDecoderSelector. Fix this. See: http://crbug.com/222054
  if (!init_cb_.is_null())
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

  state_ = STOPPED;
  // Break the ref-count loop so we don't leak objects.
  // TODO(scherkus): Make DemuxerStream and/or VideoDecoder not ref-counted so
  // we don't need this here. See: http://crbug.com/173313
  stream_ = NULL;
  decrypting_demuxer_stream_ = NULL;
  decoder_ = NULL;
  message_loop_->PostTask(FROM_HERE, base::ResetAndReturn(&stop_cb_));
}

bool VideoFrameStream::HasOutputFrameAvailable() const {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return decoder_->HasOutputFrameAvailable();
}

void VideoFrameStream::Read(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  stream_->Read(read_cb);
}

const AudioDecoderConfig& VideoFrameStream::audio_decoder_config() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  LOG(FATAL) << "Method audio_decoder_config() called on VideoFrameStream";
  return stream_->audio_decoder_config();
}

const VideoDecoderConfig& VideoFrameStream::video_decoder_config() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return stream_->video_decoder_config();
}

DemuxerStream::Type VideoFrameStream::type() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return VIDEO;
}

void VideoFrameStream::EnableBitstreamConverter() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  stream_->EnableBitstreamConverter();
}

void VideoFrameStream::OnDecoderSelected(
    scoped_ptr<VideoDecoderSelector> decoder_selector,
    const scoped_refptr<VideoDecoder>& selected_decoder,
    const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, UNINITIALIZED);
  DCHECK(!init_cb_.is_null());

  if (!selected_decoder) {
    state_ = UNINITIALIZED;
    base::ResetAndReturn(&init_cb_).Run(false, false);
  } else {
    decoder_ = selected_decoder;
    decrypting_demuxer_stream_ = decrypting_demuxer_stream;
    state_ = NORMAL;
    base::ResetAndReturn(&init_cb_).Run(true, decoder_->HasAlpha());
  }

  // Stop() called during initialization.
  if (!stop_cb_.is_null()) {
    Stop(base::ResetAndReturn(&stop_cb_));
    return;
  }
}

void VideoFrameStream::OnFrameRead(const VideoDecoder::Status status,
                                   const scoped_refptr<VideoFrame>& frame) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  DCHECK(!read_cb_.is_null());

  base::ResetAndReturn(&read_cb_).Run(status, frame);
}

void VideoFrameStream::ResetDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  DCHECK(!reset_cb_.is_null());

  decoder_->Reset(base::Bind(&VideoFrameStream::OnDecoderReset, weak_this_));
}

void VideoFrameStream::OnDecoderReset() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  // If Reset() was called during pending read, read callback should be fired
  // before the reset callback is fired.
  DCHECK(read_cb_.is_null());
  DCHECK(!reset_cb_.is_null());

  base::ResetAndReturn(&reset_cb_).Run();
}

void VideoFrameStream::StopDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  DCHECK(!stop_cb_.is_null());

  decoder_->Stop(base::Bind(&VideoFrameStream::OnDecoderStopped, weak_this_));
}

void VideoFrameStream::OnDecoderStopped() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, NORMAL);
  // If Stop() was called during pending read/reset, read/reset callback should
  // be fired before the stop callback is fired.
  DCHECK(read_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(!stop_cb_.is_null());

  state_ = STOPPED;
  // Break the ref-count loop so we don't leak objects.
  // TODO(scherkus): Make DemuxerStream and/or VideoDecoder not ref-counted so
  // we don't need this here. See: http://crbug.com/173313
  stream_ = NULL;
  decrypting_demuxer_stream_ = NULL;
  decoder_ = NULL;
  base::ResetAndReturn(&stop_cb_).Run();
}

}  // namespace media
