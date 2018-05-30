// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"

namespace media {

static bool IsStreamValid(DemuxerStream* stream) {
  return ((stream->type() == DemuxerStream::AUDIO &&
           stream->audio_decoder_config().IsValidConfig()) ||
          (stream->type() == DemuxerStream::VIDEO &&
           stream->video_decoder_config().IsValidConfig()));
}

DecryptingDemuxerStream::DecryptingDemuxerStream(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log,
    const base::Closure& waiting_for_decryption_key_cb)
    : task_runner_(task_runner),
      media_log_(media_log),
      state_(kUninitialized),
      waiting_for_decryption_key_cb_(waiting_for_decryption_key_cb),
      demuxer_stream_(NULL),
      decryptor_(NULL),
      key_added_while_decrypt_pending_(false),
      weak_factory_(this) {}

std::string DecryptingDemuxerStream::GetDisplayName() const {
  return "DecryptingDemuxerStream";
}

void DecryptingDemuxerStream::Initialize(DemuxerStream* stream,
                                         CdmContext* cdm_context,
                                         const PipelineStatusCB& status_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized) << state_;
  DCHECK(stream);
  DCHECK(cdm_context);
  DCHECK(!demuxer_stream_);

  weak_this_ = weak_factory_.GetWeakPtr();
  demuxer_stream_ = stream;
  init_cb_ = BindToCurrentLoop(status_cb);

  InitializeDecoderConfig();

  if (!cdm_context->GetDecryptor()) {
    DVLOG(2) << __func__ << ": no decryptor";
    state_ = kUninitialized;
    base::ResetAndReturn(&init_cb_).Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decryptor_ = cdm_context->GetDecryptor();

  decryptor_->RegisterNewKeyCB(
      GetDecryptorStreamType(),
      BindToCurrentLoop(
          base::Bind(&DecryptingDemuxerStream::OnKeyAdded, weak_this_)));

  state_ = kIdle;
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

void DecryptingDemuxerStream::Read(const ReadCB& read_cb) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kIdle) << state_;
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping reads are not supported.";

  read_cb_ = BindToCurrentLoop(read_cb);
  state_ = kPendingDemuxerRead;
  demuxer_stream_->Read(
      base::Bind(&DecryptingDemuxerStream::DecryptBuffer, weak_this_));
}

void DecryptingDemuxerStream::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__ << " - state: " << state_;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized) << state_;
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

AudioDecoderConfig DecryptingDemuxerStream::audio_decoder_config() {
  DCHECK(state_ != kUninitialized) << state_;
  CHECK_EQ(demuxer_stream_->type(), AUDIO);
  return audio_config_;
}

VideoDecoderConfig DecryptingDemuxerStream::video_decoder_config() {
  DCHECK(state_ != kUninitialized) << state_;
  CHECK_EQ(demuxer_stream_->type(), VIDEO);
  return video_config_;
}

DemuxerStream::Type DecryptingDemuxerStream::type() const {
  DCHECK(state_ != kUninitialized) << state_;
  return demuxer_stream_->type();
}

DemuxerStream::Liveness DecryptingDemuxerStream::liveness() const {
  DCHECK(state_ != kUninitialized) << state_;
  return demuxer_stream_->liveness();
}

void DecryptingDemuxerStream::EnableBitstreamConverter() {
  demuxer_stream_->EnableBitstreamConverter();
}

bool DecryptingDemuxerStream::SupportsConfigChanges() {
  return demuxer_stream_->SupportsConfigChanges();
}

DecryptingDemuxerStream::~DecryptingDemuxerStream() {
  DVLOG(2) << __func__ << " : state_ = " << state_;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kUninitialized)
    return;

  if (decryptor_) {
    decryptor_->CancelDecrypt(GetDecryptorStreamType());
    decryptor_ = NULL;
  }
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(PIPELINE_ERROR_ABORT);
  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
  pending_buffer_to_decrypt_ = NULL;
}

void DecryptingDemuxerStream::DecryptBuffer(
    DemuxerStream::Status status,
    scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << ": status = " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDemuxerRead) << state_;
  DCHECK(!read_cb_.is_null());
  DCHECK_EQ(buffer.get() != NULL, status == kOk) << status;

  // Even when |!reset_cb_.is_null()|, we need to pass |kConfigChanged| back to
  // the caller so that the downstream decoder can be properly reinitialized.
  if (status == kConfigChanged) {
    DVLOG(2) << "DoDecryptBuffer() - kConfigChanged.";
    DCHECK_EQ(demuxer_stream_->type() == AUDIO, audio_config_.IsValidConfig());
    DCHECK_EQ(demuxer_stream_->type() == VIDEO, video_config_.IsValidConfig());

    // Update the decoder config, which the decoder will use when it is notified
    // of kConfigChanged.
    InitializeDecoderConfig();

    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kConfigChanged, NULL);
    if (!reset_cb_.is_null())
      DoReset();
    return;
  }

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    DoReset();
    return;
  }

  if (status == kAborted || status == kError) {
    if (status == kError) {
      MEDIA_LOG(ERROR, media_log_)
          << GetDisplayName() << ": demuxer stream read error.";
    }
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(status, nullptr);
    return;
  }

  DCHECK_EQ(kOk, status);

  if (buffer->end_of_stream()) {
    DVLOG(2) << "DoDecryptBuffer() - EOS buffer.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kOk, std::move(buffer));
    return;
  }

  if (!buffer->decrypt_config()) {
    DVLOG(2) << "DoDecryptBuffer() - clear buffer.";
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kOk, std::move(buffer));
    return;
  }

  pending_buffer_to_decrypt_ = std::move(buffer);
  state_ = kPendingDecrypt;
  DecryptPendingBuffer();
}

void DecryptingDemuxerStream::DecryptPendingBuffer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPendingDecrypt) << state_;
  decryptor_->Decrypt(
      GetDecryptorStreamType(), pending_buffer_to_decrypt_,
      BindToCurrentLoop(
          base::Bind(&DecryptingDemuxerStream::DeliverBuffer, weak_this_)));
}

void DecryptingDemuxerStream::DeliverBuffer(
    Decryptor::Status status,
    scoped_refptr<DecoderBuffer> decrypted_buffer) {
  DVLOG(3) << __func__ << " - status: " << status;
  DCHECK(task_runner_->BelongsToCurrentThread());
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
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": decrypt error";
    pending_buffer_to_decrypt_ = NULL;
    state_ = kIdle;
    base::ResetAndReturn(&read_cb_).Run(kError, nullptr);
    return;
  }

  if (status == Decryptor::kNoKey) {
    std::string key_id = pending_buffer_to_decrypt_->decrypt_config()->key_id();
    std::string missing_key_id = base::HexEncode(key_id.data(), key_id.size());
    DVLOG(1) << "DeliverBuffer() - no key for key ID " << missing_key_id;
    MEDIA_LOG(INFO, media_log_)
        << GetDisplayName() << ": no key for key ID " << missing_key_id;

    if (need_to_try_again_if_nokey) {
      // The |state_| is still kPendingDecrypt.
      MEDIA_LOG(INFO, media_log_)
          << GetDisplayName() << ": key was added, resuming decrypt";
      DecryptPendingBuffer();
      return;
    }

    state_ = kWaitingForKey;
    waiting_for_decryption_key_cb_.Run();
    return;
  }

  DCHECK_EQ(status, Decryptor::kSuccess);

  // Copy the key frame flag from the encrypted to decrypted buffer, assuming
  // that the decryptor initialized the flag to false.
  if (pending_buffer_to_decrypt_->is_key_frame())
    decrypted_buffer->set_is_key_frame(true);

  pending_buffer_to_decrypt_ = NULL;
  state_ = kIdle;
  base::ResetAndReturn(&read_cb_).Run(kOk, std::move(decrypted_buffer));
}

void DecryptingDemuxerStream::OnKeyAdded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPendingDecrypt) {
    key_added_while_decrypt_pending_ = true;
    return;
  }

  if (state_ == kWaitingForKey) {
    MEDIA_LOG(INFO, media_log_)
        << GetDisplayName() << ": key was added, resuming decrypt";
    state_ = kPendingDecrypt;
    DecryptPendingBuffer();
  }
}

void DecryptingDemuxerStream::DoReset() {
  DCHECK(state_ != kUninitialized);
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
  // The decoder selector or upstream demuxer make sure the stream is valid.
  DCHECK(IsStreamValid(demuxer_stream_));

  // Since |this| is a decrypted version of |demuxer_stream_|, the decoder
  // config of |this| should always be a decrypted version of |demuxer_stream_|
  // configs.
  switch (demuxer_stream_->type()) {
    case AUDIO: {
      audio_config_ = demuxer_stream_->audio_decoder_config();
      if (audio_config_.is_encrypted())
        audio_config_.SetIsEncrypted(false);
      break;
    }

    case VIDEO: {
      video_config_ = demuxer_stream_->video_decoder_config();
      if (video_config_.is_encrypted())
        video_config_.SetIsEncrypted(false);
      break;
    }

    default:
      NOTREACHED();
      return;
  }
}

}  // namespace media
