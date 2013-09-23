// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_video_decoder.h"

namespace media {

VideoDecoderSelector::VideoDecoderSelector(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ScopedVector<VideoDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      decoders_(decoders.Pass()),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      input_stream_(NULL),
      weak_ptr_factory_(this) {
}

VideoDecoderSelector::~VideoDecoderSelector() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(select_decoder_cb_.is_null());
}

void VideoDecoderSelector::SelectVideoDecoder(
    DemuxerStream* stream,
    const SelectDecoderCB& select_decoder_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  const VideoDecoderConfig& config = stream->video_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream config.";
    ReturnNullDecoder();
    return;
  }

  input_stream_ = stream;

  if (!config.is_encrypted()) {
    InitializeDecoder();
    return;
  }

  // This could happen if Encrypted Media Extension (EME) is not enabled.
  if (set_decryptor_ready_cb_.is_null()) {
    ReturnNullDecoder();
    return;
  }

  video_decoder_.reset(new DecryptingVideoDecoder(
      message_loop_, set_decryptor_ready_cb_));

  video_decoder_->Initialize(
      input_stream_->video_decoder_config(),
      base::Bind(&VideoDecoderSelector::DecryptingVideoDecoderInitDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecoderSelector::Abort() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  // This could happen when SelectVideoDecoder() was not called or when
  // |select_decoder_cb_| was already posted but not fired (e.g. in the
  // message loop queue).
  if (select_decoder_cb_.is_null())
    return;

  // We must be trying to initialize the |video_decoder_| or the
  // |decrypted_stream_|. Invalid all weak pointers so that all initialization
  // callbacks won't fire.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (video_decoder_) {
    // |decrypted_stream_| is either NULL or already initialized. We don't
    // need to Reset() |decrypted_stream_| in either case.
    video_decoder_->Stop(base::Bind(&VideoDecoderSelector::ReturnNullDecoder,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (decrypted_stream_) {
    decrypted_stream_->Reset(
        base::Bind(&VideoDecoderSelector::ReturnNullDecoder,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NOTREACHED();
}

void VideoDecoderSelector::DecryptingVideoDecoderInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        video_decoder_.Pass(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  video_decoder_.reset();

  decrypted_stream_.reset(new DecryptingDemuxerStream(
      message_loop_, set_decryptor_ready_cb_));

  decrypted_stream_->Initialize(
      input_stream_,
      base::Bind(&VideoDecoderSelector::DecryptingDemuxerStreamInitDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecoderSelector::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    ReturnNullDecoder();
    return;
  }

  DCHECK(!decrypted_stream_->video_decoder_config().is_encrypted());
  input_stream_ = decrypted_stream_.get();
  InitializeDecoder();
}

void VideoDecoderSelector::InitializeDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!video_decoder_);

  if (decoders_.empty()) {
    ReturnNullDecoder();
    return;
  }

  video_decoder_.reset(decoders_.front());
  decoders_.weak_erase(decoders_.begin());

  video_decoder_->Initialize(input_stream_->video_decoder_config(),
                             base::Bind(&VideoDecoderSelector::DecoderInitDone,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecoderSelector::DecoderInitDone(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    video_decoder_.reset();
    InitializeDecoder();
    return;
  }

  base::ResetAndReturn(&select_decoder_cb_).Run(video_decoder_.Pass(),
                                                decrypted_stream_.Pass());
}

void VideoDecoderSelector::ReturnNullDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<VideoDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
}

}  // namespace media
