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
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    interfaces::AudioDecoderPtr remote_decoder)
    : task_runner_(task_runner),
      remote_decoder_info_(remote_decoder.PassInterface()),
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

  // Bind |remote_decoder_| to the |task_runner_|.
  remote_decoder_.Bind(std::move(remote_decoder_info_));

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
  output_cb_ = output_cb;

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
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  DCHECK(decode_cb_.is_null());
  decode_cb_ = decode_cb;

  remote_decoder_->Decode(
      TransferDecoderBuffer(buffer),
      base::Bind(&MojoAudioDecoder::OnDecodeStatus, base::Unretained(this)));
}

void MojoAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    if (!decode_cb_.is_null()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(base::ResetAndReturn(&decode_cb_),
                                        DecodeStatus::DECODE_ERROR));
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

  output_cb_.Run(buffer.To<scoped_refptr<AudioBuffer>>());
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
    base::ResetAndReturn(&decode_cb_).Run(DecodeStatus::DECODE_ERROR);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

void MojoAudioDecoder::OnInitialized(bool success,
                                     bool needs_bitstream_conversion) {
  DVLOG(1) << __FUNCTION__ << ": success:" << success;
  DCHECK(task_runner_->BelongsToCurrentThread());

  needs_bitstream_conversion_ = needs_bitstream_conversion;

  if (success)
    CreateDataPipe();

  base::ResetAndReturn(&init_cb_).Run(success);
}

static media::DecodeStatus ConvertDecodeStatus(
    interfaces::AudioDecoder::DecodeStatus status) {
  switch (status) {
    case interfaces::AudioDecoder::DecodeStatus::OK:
      return media::DecodeStatus::OK;
    case interfaces::AudioDecoder::DecodeStatus::ABORTED:
      return media::DecodeStatus::ABORTED;
    case interfaces::AudioDecoder::DecodeStatus::DECODE_ERROR:
      return media::DecodeStatus::DECODE_ERROR;
  }
  NOTREACHED();
  return media::DecodeStatus::DECODE_ERROR;
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

void MojoAudioDecoder::CreateDataPipe() {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  // TODO(timav): Consider capacity calculation based on AudioDecoderConfig.
  options.capacity_num_bytes = 512 * 1024;

  mojo::DataPipe write_pipe(options);

  // Keep producer end.
  producer_handle_ = std::move(write_pipe.producer_handle);

  // Pass consumer end to |remote_decoder_|.
  remote_decoder_->SetDataSource(std::move(write_pipe.consumer_handle));
}

interfaces::DecoderBufferPtr MojoAudioDecoder::TransferDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& media_buffer) {
  interfaces::DecoderBufferPtr buffer =
      interfaces::DecoderBuffer::From(media_buffer);
  if (media_buffer->end_of_stream())
    return buffer;

  // Serialize the data section of the DecoderBuffer into our pipe.
  uint32_t num_bytes = base::checked_cast<uint32_t>(media_buffer->data_size());
  DCHECK_GT(num_bytes, 0u);
  CHECK_EQ(WriteDataRaw(producer_handle_.get(), media_buffer->data(),
                        &num_bytes, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(media_buffer->data_size()));
  return buffer;
}

}  // namespace media
