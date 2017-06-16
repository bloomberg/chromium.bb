// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"

namespace media {

MojoVideoDecoderService::MojoVideoDecoderService(
    MojoMediaClient* mojo_media_client)
    : mojo_media_client_(mojo_media_client), weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoderService::~MojoVideoDecoderService() {}

void MojoVideoDecoderService::Construct(
    mojom::VideoDecoderClientAssociatedPtrInfo client,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    mojom::CommandBufferIdPtr command_buffer_id) {
  DVLOG(1) << __func__;

  // TODO(sandersd): Close the channel.
  if (decoder_)
    return;

  decoder_ = mojo_media_client_->CreateVideoDecoder(
      base::ThreadTaskRunnerHandle::Get(), std::move(command_buffer_id),
      base::Bind(&MojoVideoDecoderService::OnDecoderOutput, weak_this_));

  client_.Bind(std::move(client));

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(decoder_buffer_pipe)));
}

void MojoVideoDecoderService::Initialize(mojom::VideoDecoderConfigPtr config,
                                         bool low_delay,
                                         const InitializeCallback& callback) {
  DVLOG(1) << __func__;

  if (!decoder_) {
    callback.Run(false, false, 1);
    return;
  }

  decoder_->Initialize(
      config.To<VideoDecoderConfig>(), low_delay, nullptr,
      base::Bind(&MojoVideoDecoderService::OnDecoderInitialized, weak_this_,
                 callback),
      base::Bind(&MojoVideoDecoderService::OnDecoderOutput, weak_this_,
                 MojoMediaClient::ReleaseMailboxCB()));
}

void MojoVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  DVLOG(2) << __func__;

  if (!decoder_) {
    callback.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoVideoDecoderService::OnDecoderRead,
                                        weak_this_, callback));
}

void MojoVideoDecoderService::Reset(const ResetCallback& callback) {
  DVLOG(1) << __func__;

  if (!decoder_) {
    callback.Run();
    return;
  }

  decoder_->Reset(base::Bind(&MojoVideoDecoderService::OnDecoderReset,
                             weak_this_, callback));
}

void MojoVideoDecoderService::OnDecoderInitialized(
    const InitializeCallback& callback,
    bool success) {
  DVLOG(1) << __func__;
  DCHECK(decoder_);
  callback.Run(success, decoder_->NeedsBitstreamConversion(),
               decoder_->GetMaxDecodeRequests());
}

void MojoVideoDecoderService::OnDecoderRead(
    const DecodeCallback& callback,
    scoped_refptr<DecoderBuffer> buffer) {
  // TODO(sandersd): Close the channel.
  if (!buffer) {
    callback.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(
      buffer, base::Bind(&MojoVideoDecoderService::OnDecoderDecoded, weak_this_,
                         callback));
}

void MojoVideoDecoderService::OnDecoderDecoded(const DecodeCallback& callback,
                                               DecodeStatus status) {
  DVLOG(2) << __func__;
  DCHECK(decoder_);
  DCHECK(decoder_->CanReadWithoutStalling());
  callback.Run(status);
}

void MojoVideoDecoderService::OnDecoderReset(const ResetCallback& callback) {
  DVLOG(1) << __func__;
  callback.Run();
}

void MojoVideoDecoderService::OnDecoderOutput(
    MojoMediaClient::ReleaseMailboxCB release_cb,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG(2) << __func__;
  DCHECK(client_);

  base::Optional<base::UnguessableToken> release_token;
  if (release_cb) {
    release_token = base::UnguessableToken::Create();
    release_mailbox_cbs_[*release_token] = std::move(release_cb);
  }

  client_->OnVideoFrameDecoded(frame, std::move(release_token));
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

}  // namespace media
