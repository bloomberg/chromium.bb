// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    interfaces::AudioDecoderPtr remote_decoder)
    : task_runner_(task_runner),
      remote_decoder_(std::move(remote_decoder)),
      binding_(this),
      has_connection_error_(false),
      needs_bitstream_conversion_(false) {
  DVLOG(1) << __FUNCTION__;
}

MojoAudioDecoder::~MojoAudioDecoder() {
  DVLOG(1) << __FUNCTION__;
}

std::string MojoAudioDecoder::GetDisplayName() const {
  return "MojoAudioDecoder";
}

void MojoAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Fail immediately if the stream is encrypted but |cdm_context| is invalid.
  int cdm_id = (config.is_encrypted() && cdm_context)
                   ? cdm_context->GetCdmId()
                   : CdmContext::kInvalidCdmId;

  if (config.is_encrypted() && CdmContext::kInvalidCdmId == cdm_id) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  // If connection error has happened, fail immediately.
  if (remote_decoder_.encountered_error()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the error handler can't be invoked once |remote_decoder_| is destroyed.
  remote_decoder_.set_connection_error_handler(
      base::Bind(&MojoAudioDecoder::OnConnectionError, base::Unretained(this)));

  init_cb_ = init_cb;

  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the callback won't be dispatched if |remote_decoder_| is destroyed.
  remote_decoder_->Initialize(
      binding_.CreateInterfacePtrAndBind(),
      interfaces::AudioDecoderConfig::From(config), cdm_id,
      base::Bind(&MojoAudioDecoder::OnInitialized, base::Unretained(this)));
}

void MojoAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, kDecodeError));
    return;
  }

  DCHECK(decode_cb_.is_null());
  decode_cb_ = decode_cb;

  // This code won't work because |buffer| is not serialized.
  // TODO(timav): Serialize DecodeBuffer into data pipe.
  remote_decoder_->Decode(
      interfaces::DecoderBuffer::From(buffer),
      base::Bind(&MojoAudioDecoder::OnDecodeStatus, base::Unretained(this)));
}

void MojoAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    if (!decode_cb_.is_null()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(base::ResetAndReturn(&decode_cb_), kDecodeError));
    }

    task_runner_->PostTask(FROM_HERE, closure);
    return;
  }

  DCHECK(reset_cb_.is_null());
  reset_cb_ = closure;
  remote_decoder_->Reset(
      base::Bind(&MojoAudioDecoder::OnResetDone, base::Unretained(this)));
}

bool MojoAudioDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  return needs_bitstream_conversion_;
}

void MojoAudioDecoder::OnBufferDecoded(interfaces::AudioBufferPtr buffer) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

void MojoAudioDecoder::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;

  if (!init_cb_.is_null()) {
    base::ResetAndReturn(&init_cb_).Run(false);
    return;
  }

  if (!decode_cb_.is_null())
    base::ResetAndReturn(&decode_cb_).Run(kDecodeError);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

void MojoAudioDecoder::OnInitialized(bool status,
                                     bool needs_bitstream_conversion) {
  DVLOG(1) << __FUNCTION__ << ": status:" << status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  needs_bitstream_conversion_ = needs_bitstream_conversion;

  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb_, status));
}

static media::AudioDecoder::Status ConvertDecodeStatus(
    interfaces::AudioDecoder::DecodeStatus status) {
  switch (status) {
    case interfaces::AudioDecoder::DecodeStatus::OK:
      return media::AudioDecoder::kOk;
    case interfaces::AudioDecoder::DecodeStatus::ABORTED:
      return media::AudioDecoder::kAborted;
    case interfaces::AudioDecoder::DecodeStatus::DECODE_ERROR:
      return media::AudioDecoder::kDecodeError;
  }
  NOTREACHED();
  return media::AudioDecoder::kDecodeError;
}

void MojoAudioDecoder::OnDecodeStatus(
    interfaces::AudioDecoder::DecodeStatus status) {
  DVLOG(1) << __FUNCTION__ << ": status:" << status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!decode_cb_.is_null());
  base::ResetAndReturn(&decode_cb_).Run(ConvertDecodeStatus(status));
}

void MojoAudioDecoder::OnResetDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // For pending decodes OnDecodeStatus() should arrive before OnResetDone().
  DCHECK(decode_cb_.is_null());

  DCHECK(!reset_cb_.is_null());
  base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
