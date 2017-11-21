// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/overlay_info.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace media {

MojoVideoDecoder::MojoVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    mojom::VideoDecoderPtr remote_decoder,
    const RequestOverlayInfoCB& request_overlay_info_cb)
    : task_runner_(task_runner),
      remote_decoder_info_(remote_decoder.PassInterface()),
      gpu_factories_(gpu_factories),
      client_binding_(this),
      media_log_service_(media_log),
      media_log_binding_(&media_log_service_),
      request_overlay_info_cb_(request_overlay_info_cb),
      weak_factory_(this) {
  DVLOG(1) << __func__;
}

MojoVideoDecoder::~MojoVideoDecoder() {
  DVLOG(1) << __func__;
  if (request_overlay_info_cb_ && overlay_info_requested_)
    request_overlay_info_cb_.Run(false, ProvideOverlayInfoCB());
}

std::string MojoVideoDecoder::GetDisplayName() const {
  return "MojoVideoDecoder";
}

void MojoVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!weak_this_)
    weak_this_ = weak_factory_.GetWeakPtr();

  if (!remote_decoder_bound_)
    BindRemoteDecoder();

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  // Fail immediately if the stream is encrypted but |cdm_context| is invalid.
  int cdm_id = (config.is_encrypted() && cdm_context)
                   ? cdm_context->GetCdmId()
                   : CdmContext::kInvalidCdmId;

  if (config.is_encrypted() && CdmContext::kInvalidCdmId == cdm_id) {
    DVLOG(1) << __func__ << ": Invalid CdmContext.";
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  initialized_ = false;
  init_cb_ = init_cb;
  output_cb_ = output_cb;
  remote_decoder_->Initialize(
      config, low_delay, cdm_id,
      base::Bind(&MojoVideoDecoder::OnInitializeDone, base::Unretained(this)));
}

void MojoVideoDecoder::OnInitializeDone(bool status,
                                        bool needs_bitstream_conversion,
                                        int32_t max_decode_requests) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  initialized_ = status;
  needs_bitstream_conversion_ = needs_bitstream_conversion;
  max_decode_requests_ = max_decode_requests;
  base::ResetAndReturn(&init_cb_).Run(status);
}

void MojoVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  uint64_t decode_id = decode_counter_++;
  pending_decodes_[decode_id] = decode_cb;
  remote_decoder_->Decode(std::move(mojo_buffer),
                          base::Bind(&MojoVideoDecoder::OnDecodeDone,
                                     base::Unretained(this), decode_id));
}

void MojoVideoDecoder::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const base::Optional<base::UnguessableToken>& release_token) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(sandersd): Prove that all paths read this value again after running
  // |output_cb_|. In practice this isn't very important, since all decoders
  // running via MojoVideoDecoder currently use a static value.
  can_read_without_stalling_ = can_read_without_stalling;

  if (release_token) {
    frame->SetReleaseMailboxCB(
        BindToCurrentLoop(base::Bind(&MojoVideoDecoder::OnReleaseMailbox,
                                     weak_this_, release_token.value())));
  }
  output_cb_.Run(frame);
}

void MojoVideoDecoder::OnReleaseMailbox(
    const base::UnguessableToken& release_token,
    const gpu::SyncToken& release_sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  remote_decoder_->OnReleaseMailbox(release_token, release_sync_token);
}

void MojoVideoDecoder::OnDecodeDone(uint64_t decode_id, DecodeStatus status) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = pending_decodes_.find(decode_id);
  if (it == pending_decodes_.end()) {
    DLOG(ERROR) << "Decode request " << decode_id << " not found";
    Stop();
    return;
  }
  DecodeCB decode_cb = it->second;
  pending_decodes_.erase(it);
  decode_cb.Run(status);
}

void MojoVideoDecoder::Reset(const base::Closure& reset_cb) {
  DVLOG(1) << __func__;
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
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&reset_cb_).Run();
}

bool MojoVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(3) << __func__;
  DCHECK(initialized_);
  return needs_bitstream_conversion_;
}

bool MojoVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(3) << __func__;
  return can_read_without_stalling_;
}

int MojoVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(3) << __func__;
  DCHECK(initialized_);
  return max_decode_requests_;
}

void MojoVideoDecoder::BindRemoteDecoder() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!remote_decoder_bound_);

  remote_decoder_.Bind(std::move(remote_decoder_info_));
  remote_decoder_bound_ = true;

  remote_decoder_.set_connection_error_handler(
      base::Bind(&MojoVideoDecoder::Stop, base::Unretained(this)));

  mojom::VideoDecoderClientAssociatedPtrInfo client_ptr_info;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr_info));

  mojom::MediaLogAssociatedPtrInfo media_log_ptr_info;
  media_log_binding_.Bind(mojo::MakeRequest(&media_log_ptr_info));

  // TODO(sandersd): Better buffer sizing.
  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      DemuxerStream::VIDEO, &remote_consumer_handle);

  media::mojom::CommandBufferIdPtr command_buffer_id;
  if (gpu_factories_) {
    base::UnguessableToken channel_token = gpu_factories_->GetChannelToken();
    if (channel_token) {
      command_buffer_id = media::mojom::CommandBufferId::New();
      command_buffer_id->channel_token = std::move(channel_token);
      command_buffer_id->route_id = gpu_factories_->GetCommandBufferRouteId();
    }
  }

  remote_decoder_->Construct(
      std::move(client_ptr_info), std::move(media_log_ptr_info),
      std::move(remote_consumer_handle), std::move(command_buffer_id));
}

void MojoVideoDecoder::RequestOverlayInfo(bool restart_for_transitions) {
  DVLOG(2) << __func__;
  DCHECK(request_overlay_info_cb_);
  overlay_info_requested_ = true;
  request_overlay_info_cb_.Run(
      restart_for_transitions,
      BindToCurrentLoop(base::Bind(&MojoVideoDecoder::OnOverlayInfoChanged,
                                   weak_factory_.GetWeakPtr())));
}

void MojoVideoDecoder::OnOverlayInfoChanged(const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  if (has_connection_error_)
    return;
  remote_decoder_->OnOverlayInfoChanged(overlay_info);
}

void MojoVideoDecoder::Stop() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);

  for (const auto& pending_decode : pending_decodes_)
    pending_decode.second.Run(DecodeStatus::DECODE_ERROR);
  pending_decodes_.clear();

  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
