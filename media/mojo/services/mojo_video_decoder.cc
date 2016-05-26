// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

MojoVideoDecoder::MojoVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    mojom::VideoDecoderPtr remote_decoder)
    : task_runner_(task_runner),
      gpu_factories_(gpu_factories),
      remote_decoder_info_(remote_decoder.PassInterface()),
      binding_(this) {
  (void)gpu_factories_;
  DVLOG(1) << __FUNCTION__;
}

MojoVideoDecoder::~MojoVideoDecoder() {
  DVLOG(1) << __FUNCTION__;
}

std::string MojoVideoDecoder::GetDisplayName() const {
  return "MojoVideoDecoder";
}

void MojoVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!cdm_context);

  if (!remote_decoder_bound_)
    BindRemoteDecoder();

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  init_cb_ = init_cb;
  output_cb_ = output_cb;
  remote_decoder_->Initialize(
      mojom::VideoDecoderConfig::From(config), low_delay,
      base::Bind(&MojoVideoDecoder::OnInitializeDone, base::Unretained(this)));
}

// TODO(sandersd): Remove this indirection once a working decoder has been
// brought up.
void MojoVideoDecoder::OnInitializeDone(bool status) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&init_cb_).Run(status);
}

void MojoVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  mojom::DecoderBufferPtr mojo_buffer = mojom::DecoderBuffer::From(buffer);

  // TODO(sandersd): Destruct cleanly on error.
  if (!buffer->end_of_stream()) {
    uint32_t data_size = base::checked_cast<uint32_t>(buffer->data_size());
    DCHECK_GT(data_size, 0u);
    uint32_t bytes_written = data_size;
    CHECK_EQ(WriteDataRaw(decoder_buffer_pipe_.get(), buffer->data(),
                          &bytes_written, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(bytes_written, data_size);
  }

  // TODO(sandersd): Support more than one decode at a time.
  decode_cb_ = decode_cb;
  remote_decoder_->Decode(
      std::move(mojo_buffer),
      base::Bind(&MojoVideoDecoder::OnDecodeDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnVideoFrameDecoded(mojom::VideoFramePtr frame) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  output_cb_.Run(frame.To<scoped_refptr<VideoFrame>>());
}

void MojoVideoDecoder::OnDecodeDone(mojom::DecodeStatus status) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&decode_cb_).Run(static_cast<DecodeStatus>(status));
}

void MojoVideoDecoder::Reset(const base::Closure& reset_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, reset_cb);
    return;
  }

  reset_cb_ = reset_cb;
  remote_decoder_->Reset(
      base::Bind(&MojoVideoDecoder::OnResetDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnResetDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&reset_cb_).Run();
}

bool MojoVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __FUNCTION__;
  return false;
}

bool MojoVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(1) << __FUNCTION__;
  return true;
}

int MojoVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(1) << __FUNCTION__;
  return 1;
}

void MojoVideoDecoder::BindRemoteDecoder() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!remote_decoder_bound_);

  remote_decoder_.Bind(std::move(remote_decoder_info_));
  remote_decoder_bound_ = true;

  if (remote_decoder_.encountered_error()) {
    has_connection_error_ = true;
    return;
  }

  remote_decoder_.set_connection_error_handler(
      base::Bind(&MojoVideoDecoder::OnConnectionError, base::Unretained(this)));

  // TODO(sandersd): Better buffer sizing.
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 2 * 1024 * 1024;
  mojo::DataPipe decoder_buffer_pipe(options);

  decoder_buffer_pipe_ = std::move(decoder_buffer_pipe.producer_handle);
  remote_decoder_->Construct(binding_.CreateInterfacePtrAndBind(),
                             std::move(decoder_buffer_pipe.consumer_handle));
}

void MojoVideoDecoder::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;

  // TODO(sandersd): Write a wrapper class (like BindToCurrentLoop) that handles
  // the lifetime of callbacks like this.
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);
  // TODO(sandersd): If there is a pending reset, should these be aborted?
  if (!decode_cb_.is_null())
    base::ResetAndReturn(&decode_cb_).Run(DecodeStatus::DECODE_ERROR);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
