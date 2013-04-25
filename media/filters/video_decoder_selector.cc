// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
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

VideoDecoderSelector::~VideoDecoderSelector() {}

void VideoDecoderSelector::SelectVideoDecoder(
    DemuxerStream* stream,
    const StatisticsCB& statistics_cb,
    const SelectDecoderCB& select_decoder_cb) {
  DVLOG(2) << "SelectVideoDecoder()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  const VideoDecoderConfig& config = stream->video_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream config.";
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<VideoDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  input_stream_ = stream;
  statistics_cb_ = statistics_cb;

  if (!config.is_encrypted()) {
    InitializeDecoder(decoders_.begin());
    return;
  }

  // This could happen if Encrypted Media Extension (EME) is not enabled.
  if (set_decryptor_ready_cb_.is_null()) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<VideoDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  video_decoder_.reset(new DecryptingVideoDecoder(
      message_loop_, set_decryptor_ready_cb_));

  video_decoder_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &VideoDecoderSelector::DecryptingVideoDecoderInitDone,
          weak_ptr_factory_.GetWeakPtr())),
      statistics_cb_);
}

void VideoDecoderSelector::DecryptingVideoDecoderInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        video_decoder_.Pass(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  decrypted_stream_.reset(new DecryptingDemuxerStream(
      message_loop_, set_decryptor_ready_cb_));

  decrypted_stream_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &VideoDecoderSelector::DecryptingDemuxerStreamInitDone,
          weak_ptr_factory_.GetWeakPtr())));
}

void VideoDecoderSelector::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    decrypted_stream_.reset();
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<VideoDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  DCHECK(!decrypted_stream_->video_decoder_config().is_encrypted());
  input_stream_ = decrypted_stream_.get();
  InitializeDecoder(decoders_.begin());
}

void VideoDecoderSelector::InitializeDecoder(
    ScopedVector<VideoDecoder>::iterator iter) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (iter == decoders_.end()) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<VideoDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  (*iter)->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &VideoDecoderSelector::DecoderInitDone,
          weak_ptr_factory_.GetWeakPtr(),
          iter)),
      statistics_cb_);
}

void VideoDecoderSelector::DecoderInitDone(
    ScopedVector<VideoDecoder>::iterator iter, PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    InitializeDecoder(++iter);
    return;
  }

  scoped_ptr<VideoDecoder> video_decoder(*iter);
  decoders_.weak_erase(iter);

  base::ResetAndReturn(&select_decoder_cb_).Run(video_decoder.Pass(),
                                                decrypted_stream_.Pass());
}

}  // namespace media
