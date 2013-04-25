// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"

namespace media {

#define BIND_TO_LOOP(function) \
    media::BindToLoop(message_loop_, base::Bind(function, weak_this_))

static bool IsStreamValidAndEncrypted(DemuxerStream* stream) {
  return ((stream->type() == DemuxerStream::AUDIO &&
           stream->audio_decoder_config().IsValidConfig() &&
           stream->audio_decoder_config().is_encrypted()) ||
          (stream->type() == DemuxerStream::VIDEO &&
           stream->video_decoder_config().IsValidConfig() &&
           stream->video_decoder_config().is_encrypted()));
}

DecryptingDemuxerStream::DecryptingDemuxerStream(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      weak_factory_(this),
      state_(kUninitialized),
      demuxer_stream_(NULL),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      decryptor_(NULL),
      key_added_while_decrypt_pending_(false) {
}

void DecryptingDemuxerStream::Initialize(
    DemuxerStream* stream,
    const PipelineStatusCB& status_cb) {
  DVLOG(2) << "Initialize()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;

  DCHECK(!demuxer_stream_);
  weak_this_ = weak_factory_.GetWeakPtr();
  demuxer_stream_ = stream;
  init_cb_ = status_cb;

  InitializeDecoderConfig();

  state_ = kDecryptorRequested;
  set_decryptor_ready_cb_.Run(
      BIND_TO_LOOP(&DecryptingDemuxerStream::SetDecryptor));
}

void DecryptingDemuxerStream::Read(const ReadCB& read_cb) {
  DVLOG(3) << "Read()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kIdle) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping reads are not supported.";

  read_cb_ = read_cb;
  state_ = kPendingDemuxerRead;
  demuxer_stream_->Read(
      base::Bind(&DecryptingDemuxerStream::DecryptBuffer, weak_this_));
}

void DecryptingDemuxerStream::Reset(const base::Closure& closure) {
  DVLOG(2) << "Reset() - state: " << state_;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  DCHECK(init_cb_.is_null());  // No Reset() during pending initialization.
  DCHECK(reset_cb_.is_null());

  reset_cb_ = BindToCurrentLoop(closure);

  decryptor_->CancelDecrypt(GetDecryptorStreamType());

  // Reset() cannot complete if the read callback is still pending.
  // Defer the resetting process in this case. The |reset_cb_| will be fired
  // after the read callback is fired - see DoDecryptBuffer() and
  // DoDeliverBuffer().
  if (state_ == kPendingDemuxerRead || state_ == kPendingDecrypt) {
    DCHECK(!read_cb_.is_null());
    return;
  }

  if (state_ == kWaitingForKey) {
    DCHECK(!read_cb_.is_null());
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  }

  DCHECK(read_cb_.is_null());
  DoReset();
}

const AudioDecoderConfig& DecryptingDemuxerStream::audio_decoder_config() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  CHECK_EQ(demuxer_stream_->type(), AUDIO);
  return *audio_config_;
}

const VideoDecoderConfig& DecryptingDemuxerStream::video_decoder_config() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  CHECK_EQ(demuxer_stream_->type(), VIDEO);
  return *video_config_;
}

DemuxerStream::Type DecryptingDemuxerStream::type() {
  DCHECK(state_ != kUninitialized && state_ != kDecryptorRequested) << state_;
  return demuxer_stream_->type();
}

void DecryptingDemuxerStream::EnableBitstreamConverter() {
  demuxer_stream_->EnableBitstreamConverter();
}

DecryptingDemuxerStream::~DecryptingDemuxerStream() {}

void DecryptingDemuxerStream::SetDecryptor(Decryptor* decryptor) {
  DVLOG(2) << "SetDecryptor()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDecryptorRequested) << state_;
  DCHECK(!init_cb_.is_null());
  DCHECK(!set_decryptor_ready_cb_.is_null());

  set_decryptor_ready_cb_.Reset();
  decryptor_ = decryptor;

  decryptor_->RegisterNewKeyCB(
      GetDecryptorStreamType(),
      BIND_TO_LOOP(&DecryptingDemuxerStream::OnKeyAdded));

  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingDemuxerStream::DecryptBuffer(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(3) << "DecryptBuffer()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer != NULL, status == kOk) << status;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  if (status == kAborted) {
    DVLOG(2) << "DoDecryptBuffer() - kAborted.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == kConfigChanged) {
    DVLOG(2) << "DoDecryptBuffer() - kConfigChanged.";
    DCHECK_EQ(demuxer_stream_->type() == AUDIO, audio_config_ != NULL);
    DCHECK_EQ(demuxer_stream_->type() == VIDEO, video_config_ != NULL);

    // Update the decoder config, which the decoder will use when it is notified
    // of kConfigChanged.
    InitializeDecoderConfig();
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kConfigChanged, NULL);
    return;
  }

  if (buffer->IsEndOfStream()) {
    DVLOG(2) << "DoDecryptBuffer() - EOS buffer.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(status, buffer);
    return;
  }

  pending_buffer_to_decrypt_ = buffer;
  state_ = kPendingDecrypt;
  DecryptPendingBuffer();
}

void DecryptingDemuxerStream::DecryptPendingBuffer() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  decryptor_->Decrypt(
      GetDecryptorStreamType(),
      pending_buffer_to_decrypt_,
      BIND_TO_LOOP(&DecryptingDemuxerStream::DeliverBuffer));
}

void DecryptingDemuxerStream::DeliverBuffer(
    Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& decrypted_buffer) {
  DVLOG(3) << "DeliverBuffer() - status: " << status;
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  DCHECK_NE(status, Decryptor::kNeedMoreData);
  DCHECK(!read_cb_.is_null());
  DCHECK(pending_buffer_to_decrypt_);

  bool need_to_try_again_if_nokey = key_added_while_decrypt_pending_;
  key_added_while_decrypt_pending_ = false;

  if (!reset_cb_.is_null()) {
    pending_buffer_to_decrypt_ = NULL;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  DCHECK_EQ(status == Decryptor::kSuccess, decrypted_buffer.get() != NULL);

  if (status == Decryptor::kError) {
    DVLOG(2) << "DoDeliverBuffer() - kError";
    pending_buffer_to_decrypt_ = NULL;
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == Decryptor::kNoKey) {
    DVLOG(2) << "DoDeliverBuffer() - kNoKey";
    if (need_to_try_again_if_nokey) {
      // The |state_| is still kPendingDecrypt.
      DecryptPendingBuffer();
      return;
    }

    state_ = kWaitingForKey;
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);
  pending_buffer_to_decrypt_ = NULL;
  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, decrypted_buffer);
}

void DecryptingDemuxerStream::OnKeyAdded() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ == kPendingDecrypt) {
    key_added_while_decrypt_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    state_ = kPendingDecrypt;
    DecryptPendingBuffer();
  }
}

void DecryptingDemuxerStream::DoReset() {
  DCHECK(init_cb_.is_null());
  DCHECK(read_cb_.is_null());
  state_ = kIdle;
  base::ResetAndReturn(&reset_cb_).Run();
}

Decryptor::StreamType DecryptingDemuxerStream::GetDecryptorStreamType() const {
  if (demuxer_stream_->type() == AUDIO)
    return Decryptor::kAudio;

  DCHECK_EQ(demuxer_stream_->type(), VIDEO);
  return Decryptor::kVideo;
}

void DecryptingDemuxerStream::InitializeDecoderConfig() {
  // The decoder selector or upstream demuxer make sure the stream is valid and
  // potentially encrypted.
  DCHECK(IsStreamValidAndEncrypted(demuxer_stream_));

  switch (demuxer_stream_->type()) {
    case AUDIO: {
      const AudioDecoderConfig& input_audio_config =
          demuxer_stream_->audio_decoder_config();
      audio_config_.reset(new AudioDecoderConfig());
      audio_config_->Initialize(input_audio_config.codec(),
                                input_audio_config.sample_format(),
                                input_audio_config.channel_layout(),
                                input_audio_config.samples_per_second(),
                                input_audio_config.extra_data(),
                                input_audio_config.extra_data_size(),
                                false,  // Output audio is not encrypted.
                                false);
      break;
    }

    case VIDEO: {
      const VideoDecoderConfig& input_video_config =
          demuxer_stream_->video_decoder_config();
      video_config_.reset(new VideoDecoderConfig());
      video_config_->Initialize(input_video_config.codec(),
                                input_video_config.profile(),
                                input_video_config.format(),
                                input_video_config.coded_size(),
                                input_video_config.visible_rect(),
                                input_video_config.natural_size(),
                                input_video_config.extra_data(),
                                input_video_config.extra_data_size(),
                                false,  // Output video is not encrypted.
                                false);
      break;
    }

    default:
      NOTREACHED();
      return;
  }
}

}  // namespace media
