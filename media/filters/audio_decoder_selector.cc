// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
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
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

AudioDecoderSelector::~AudioDecoderSelector() {}

void AudioDecoderSelector::SelectAudioDecoder(
    const scoped_refptr<DemuxerStream>& stream,
    const StatisticsCB& statistics_cb,
    const SelectDecoderCB& select_decoder_cb) {
  DVLOG(2) << "SelectAudioDecoder()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  const AudioDecoderConfig& config = stream->audio_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream config.";
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<AudioDecoder>(), NULL);
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
        scoped_ptr<AudioDecoder>(), NULL);
    return;
  }

  audio_decoder_.reset(new DecryptingAudioDecoder(
      message_loop_, set_decryptor_ready_cb_));

  audio_decoder_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &AudioDecoderSelector::DecryptingAudioDecoderInitDone,
          weak_ptr_factory_.GetWeakPtr())),
      statistics_cb_);
}

void AudioDecoderSelector::DecryptingAudioDecoderInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    base::ResetAndReturn(&select_decoder_cb_).Run(audio_decoder_.Pass(), NULL);
    return;
  }

  audio_decoder_.reset();

  decrypted_stream_ = new DecryptingDemuxerStream(
      message_loop_, set_decryptor_ready_cb_);

  decrypted_stream_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &AudioDecoderSelector::DecryptingDemuxerStreamInitDone,
          weak_ptr_factory_.GetWeakPtr())));
}

void AudioDecoderSelector::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    decrypted_stream_ = NULL;
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<AudioDecoder>(), NULL);
    return;
  }

  DCHECK(!decrypted_stream_->audio_decoder_config().is_encrypted());
  input_stream_ = decrypted_stream_;
  InitializeDecoder(decoders_.begin());
}

void AudioDecoderSelector::InitializeDecoder(
    ScopedVector<AudioDecoder>::iterator iter) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (iter == decoders_.end()) {
    base::ResetAndReturn(&select_decoder_cb_).Run(
        scoped_ptr<AudioDecoder>(), NULL);
    return;
  }

  (*iter)->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &AudioDecoderSelector::DecoderInitDone,
          weak_ptr_factory_.GetWeakPtr(),
          iter)),
      statistics_cb_);
}

void AudioDecoderSelector::DecoderInitDone(
    ScopedVector<AudioDecoder>::iterator iter, PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    InitializeDecoder(++iter);
    return;
  }

  scoped_ptr<AudioDecoder> audio_decoder(*iter);
  decoders_.weak_erase(iter);

  base::ResetAndReturn(&select_decoder_cb_).Run(audio_decoder.Pass(),
                                                decrypted_stream_);
}

}  // namespace media
