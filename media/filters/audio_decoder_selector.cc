// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

AudioDecoderSelector::AudioDecoderSelector(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    ScopedVector<AudioDecoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      decoders_(decoders.Pass()),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      input_stream_(NULL),
      weak_ptr_factory_(this) {
}

AudioDecoderSelector::~AudioDecoderSelector() {
  DVLOG(2) << __FUNCTION__;
}

void AudioDecoderSelector::SelectAudioDecoder(
    DemuxerStream* stream,
    const StatisticsCB& statistics_cb,
    const SelectDecoderCB& select_decoder_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  const AudioDecoderConfig& config = stream->audio_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream config.";
    ReturnNullDecoder();
    return;
  }

  input_stream_ = stream;
  statistics_cb_ = statistics_cb;

  if (!config.is_encrypted()) {
    InitializeDecoder();
    return;
  }

  // This could happen if Encrypted Media Extension (EME) is not enabled.
  if (set_decryptor_ready_cb_.is_null()) {
    ReturnNullDecoder();
    return;
  }

  audio_decoder_.reset(new DecryptingAudioDecoder(
      message_loop_, set_decryptor_ready_cb_));

  audio_decoder_->Initialize(
      input_stream_,
      base::Bind(&AudioDecoderSelector::DecryptingAudioDecoderInitDone,
                 weak_ptr_factory_.GetWeakPtr()),
      statistics_cb_);
}

void AudioDecoderSelector::Abort() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  // This could happen when SelectAudioDecoder() was not called or when
  // |select_decoder_cb_| was already posted but not fired (e.g. in the
  // message loop queue).
  if (select_decoder_cb_.is_null())
    return;

  // We must be trying to initialize the |audio_decoder_| or the
  // |decrypted_stream_|. Invalid all weak pointers so that all initialization
  // callbacks won't fire.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (audio_decoder_) {
    // AudioDecoder doesn't provide a Stop() method. Also, |decrypted_stream_|
    // is either NULL or already initialized. We don't need to Reset()
    // |decrypted_stream_| in either case.
    ReturnNullDecoder();
    return;
  }

  if (decrypted_stream_) {
    decrypted_stream_->Reset(
        base::Bind(&AudioDecoderSelector::ReturnNullDecoder,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NOTREACHED();
}

void AudioDecoderSelector::DecryptingAudioDecoderInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        audio_decoder_.Pass(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  audio_decoder_.reset();

  decrypted_stream_.reset(new DecryptingDemuxerStream(
      message_loop_, set_decryptor_ready_cb_));

  decrypted_stream_->Initialize(
      input_stream_,
      base::Bind(&AudioDecoderSelector::DecryptingDemuxerStreamInitDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AudioDecoderSelector::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    ReturnNullDecoder();
    return;
  }

  DCHECK(!decrypted_stream_->audio_decoder_config().is_encrypted());
  input_stream_ = decrypted_stream_.get();
  InitializeDecoder();
}

void AudioDecoderSelector::InitializeDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!audio_decoder_);

  if (decoders_.empty()) {
    ReturnNullDecoder();
    return;
  }

  audio_decoder_.reset(decoders_.front());
  decoders_.weak_erase(decoders_.begin());

  audio_decoder_->Initialize(input_stream_,
                             base::Bind(&AudioDecoderSelector::DecoderInitDone,
                                        weak_ptr_factory_.GetWeakPtr()),
                             statistics_cb_);
}

void AudioDecoderSelector::DecoderInitDone(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    audio_decoder_.reset();
    InitializeDecoder();
    return;
  }

  base::ResetAndReturn(&select_decoder_cb_).Run(audio_decoder_.Pass(),
                                                decrypted_stream_.Pass());
}

void AudioDecoderSelector::ReturnNullDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::ResetAndReturn(&select_decoder_cb_).Run(
      scoped_ptr<AudioDecoder>(), scoped_ptr<DecryptingDemuxerStream>());
}

}  // namespace media
