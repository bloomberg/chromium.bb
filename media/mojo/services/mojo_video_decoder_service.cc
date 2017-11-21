// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_media_log.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"

namespace media {

MojoVideoDecoderService::MojoVideoDecoderService(
    MojoMediaClient* mojo_media_client,
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context)
    : mojo_media_client_(mojo_media_client),
      mojo_cdm_service_context_(std::move(mojo_cdm_service_context)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoderService::~MojoVideoDecoderService() {}

void MojoVideoDecoderService::Construct(
    mojom::VideoDecoderClientAssociatedPtrInfo client,
    mojom::MediaLogAssociatedPtrInfo media_log,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    mojom::CommandBufferIdPtr command_buffer_id) {
  DVLOG(1) << __func__;

  if (decoder_) {
    // TODO(sandersd): Close the channel.
    return;
  }

  client_.Bind(std::move(client));

  mojom::MediaLogAssociatedPtr media_log_ptr;
  media_log_ptr.Bind(std::move(media_log));
  media_log_ = base::MakeUnique<MojoMediaLog>(std::move(media_log_ptr));

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(decoder_buffer_pipe)));

  decoder_ = mojo_media_client_->CreateVideoDecoder(
      base::ThreadTaskRunnerHandle::Get(), media_log_.get(),
      std::move(command_buffer_id),
      base::Bind(&MojoVideoDecoderService::OnDecoderOutputWithReleaseCB,
                 weak_this_),
      base::Bind(&MojoVideoDecoderService::OnDecoderRequestedOverlayInfo,
                 weak_this_));
}

void MojoVideoDecoderService::Initialize(const VideoDecoderConfig& config,
                                         bool low_delay,
                                         int32_t cdm_id,
                                         InitializeCallback callback) {
  DVLOG(1) << __func__;

  if (!decoder_) {
    std::move(callback).Run(false, false, 1);
    return;
  }

  // Get CdmContext from cdm_id if the stream is encrypted.
  // TODO(xhwang): This code is duplicated in mojo_audio_decoder_service.
  // crbug.com/786736 .
  CdmContext* cdm_context = nullptr;
  scoped_refptr<ContentDecryptionModule> cdm;
  if (config.is_encrypted()) {
    if (!mojo_cdm_service_context_) {
      DVLOG(1) << "CDM service context not available.";
      std::move(callback).Run(false, false, 1);
      return;
    }

    cdm = mojo_cdm_service_context_->GetCdm(cdm_id);
    if (!cdm) {
      DVLOG(1) << "CDM not found for CDM id: " << cdm_id;
      std::move(callback).Run(false, false, 1);
      return;
    }

    cdm_context = cdm->GetCdmContext();
    if (!cdm_context) {
      DVLOG(1) << "CDM context not available for CDM id: " << cdm_id;
      std::move(callback).Run(false, false, 1);
      return;
    }
  }

  decoder_->Initialize(
      config, low_delay, cdm_context,
      base::Bind(&MojoVideoDecoderService::OnDecoderInitialized, weak_this_,
                 base::Passed(&callback), cdm),
      base::Bind(&MojoVideoDecoderService::OnDecoderOutput, weak_this_,
                 base::nullopt));
}

void MojoVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     DecodeCallback callback) {
  DVLOG(2) << __func__ << " pts=" << buffer->timestamp.InMilliseconds();
  if (!decoder_) {
    std::move(callback).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoVideoDecoderService::OnDecoderRead,
                                        weak_this_, std::move(callback)));
}

void MojoVideoDecoderService::Reset(ResetCallback callback) {
  DVLOG(1) << __func__;

  if (!decoder_) {
    std::move(callback).Run();
    return;
  }

  decoder_->Reset(base::Bind(&MojoVideoDecoderService::OnDecoderReset,
                             weak_this_, base::Passed(&callback)));
}

void MojoVideoDecoderService::OnDecoderInitialized(
    InitializeCallback callback,
    scoped_refptr<ContentDecryptionModule> cdm,
    bool success) {
  DVLOG(1) << __func__;
  DCHECK(decoder_);

  if (success)
    cdm_ = std::move(cdm);

  std::move(callback).Run(success, decoder_->NeedsBitstreamConversion(),
                          decoder_->GetMaxDecodeRequests());
}

void MojoVideoDecoderService::OnDecoderRead(
    DecodeCallback callback,
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    // TODO(sandersd): Close the channel.
    std::move(callback).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(
      buffer, base::Bind(&MojoVideoDecoderService::OnDecoderDecoded, weak_this_,
                         base::Passed(&callback)));
}

void MojoVideoDecoderService::OnDecoderDecoded(DecodeCallback callback,
                                               DecodeStatus status) {
  DVLOG(2) << __func__;
  std::move(callback).Run(status);
}

void MojoVideoDecoderService::OnDecoderReset(ResetCallback callback) {
  DVLOG(1) << __func__;
  std::move(callback).Run();
}

void MojoVideoDecoderService::OnDecoderOutputWithReleaseCB(
    MojoMediaClient::ReleaseMailboxCB release_cb,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG(2) << __func__ << " pts=" << frame->timestamp().InMilliseconds();
  DCHECK(client_);
  DCHECK(decoder_);

  base::Optional<base::UnguessableToken> release_token;
  if (release_cb) {
    release_token = base::UnguessableToken::Create();
    release_mailbox_cbs_[*release_token] = std::move(release_cb);
  }
  OnDecoderOutput(std::move(release_token), frame);
}

void MojoVideoDecoderService::OnDecoderOutput(
    base::Optional<base::UnguessableToken> release_token,
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(client_);
  DCHECK(decoder_);
  client_->OnVideoFrameDecoded(frame, decoder_->CanReadWithoutStalling(),
                               std::move(release_token));
}

void MojoVideoDecoderService::OnReleaseMailbox(
    const base::UnguessableToken& release_token,
    const gpu::SyncToken& release_sync_token) {
  DVLOG(2) << __func__;

  // TODO(sandersd): Is it a serious error for the client to call
  // OnReleaseMailbox() with an invalid |release_token|?
  auto it = release_mailbox_cbs_.find(release_token);
  if (it == release_mailbox_cbs_.end())
    return;

  MojoMediaClient::ReleaseMailboxCB cb = std::move(it->second);
  release_mailbox_cbs_.erase(it);
  std::move(cb).Run(release_sync_token);
}

void MojoVideoDecoderService::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK(client_);
  DCHECK(decoder_);
  DCHECK(provide_overlay_info_cb_);
  provide_overlay_info_cb_.Run(overlay_info);
}

void MojoVideoDecoderService::OnDecoderRequestedOverlayInfo(
    bool restart_for_transitions,
    const ProvideOverlayInfoCB& provide_overlay_info_cb) {
  DVLOG(2) << __func__;
  DCHECK(client_);
  DCHECK(decoder_);
  DCHECK(!provide_overlay_info_cb_);

  provide_overlay_info_cb_ = std::move(provide_overlay_info_cb);
  client_->RequestOverlayInfo(restart_for_transitions);
}

}  // namespace media
