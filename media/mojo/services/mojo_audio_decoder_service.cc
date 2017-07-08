// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

MojoAudioDecoderService::MojoAudioDecoderService(
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
    std::unique_ptr<media::AudioDecoder> decoder)
    : mojo_cdm_service_context_(mojo_cdm_service_context),
      decoder_(std::move(decoder)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioDecoderService::~MojoAudioDecoderService() {}

void MojoAudioDecoderService::Construct(
    mojom::AudioDecoderClientAssociatedPtrInfo client) {
  DVLOG(1) << __func__;
  client_.Bind(std::move(client));
}

void MojoAudioDecoderService::Initialize(const AudioDecoderConfig& config,
                                         int32_t cdm_id,
                                         const InitializeCallback& callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();

  // Get CdmContext from cdm_id if the stream is encrypted.
  CdmContext* cdm_context = nullptr;
  scoped_refptr<ContentDecryptionModule> cdm;
  if (config.is_encrypted()) {
    if (!mojo_cdm_service_context_) {
      DVLOG(1) << "CDM service context not available.";
      callback.Run(false, false);
      return;
    }

    cdm = mojo_cdm_service_context_->GetCdm(cdm_id);
    if (!cdm) {
      DVLOG(1) << "CDM not found for CDM id: " << cdm_id;
      callback.Run(false, false);
      return;
    }

    cdm_context = cdm->GetCdmContext();
    if (!cdm_context) {
      DVLOG(1) << "CDM context not available for CDM id: " << cdm_id;
      callback.Run(false, false);
      return;
    }
  }

  decoder_->Initialize(
      config, cdm_context,
      base::Bind(&MojoAudioDecoderService::OnInitialized, weak_this_, callback,
                 cdm),
      base::Bind(&MojoAudioDecoderService::OnAudioBufferReady, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __func__;

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(receive_pipe)));
}

void MojoAudioDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  DVLOG(3) << __func__;
  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoAudioDecoderService::OnReadDone,
                                        weak_this_, callback));
}

void MojoAudioDecoderService::Reset(const ResetCallback& callback) {
  DVLOG(1) << __func__;
  decoder_->Reset(
      base::Bind(&MojoAudioDecoderService::OnResetDone, weak_this_, callback));
}

void MojoAudioDecoderService::OnInitialized(
    const InitializeCallback& callback,
    scoped_refptr<ContentDecryptionModule> cdm,
    bool success) {
  DVLOG(1) << __func__ << " success:" << success;

  if (success) {
    cdm_ = cdm;
    callback.Run(success, decoder_->NeedsBitstreamConversion());
  } else {
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    callback.Run(false, false);
  }
}

// The following methods are needed so that we can bind them with a weak pointer
// to avoid running the |callback| after connection error happens and |this| is
// deleted. It's not safe to run the |callback| after a connection error.

void MojoAudioDecoderService::OnReadDone(const DecodeCallback& callback,
                                         scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << " success:" << !!buffer;

  if (!buffer) {
    callback.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(buffer, base::Bind(&MojoAudioDecoderService::OnDecodeStatus,
                                      weak_this_, callback));
}

void MojoAudioDecoderService::OnDecodeStatus(const DecodeCallback& callback,
                                             media::DecodeStatus status) {
  DVLOG(3) << __func__ << " status:" << status;
  callback.Run(status);
}

void MojoAudioDecoderService::OnResetDone(const ResetCallback& callback) {
  DVLOG(1) << __func__;
  callback.Run();
}

void MojoAudioDecoderService::OnAudioBufferReady(
    const scoped_refptr<AudioBuffer>& audio_buffer) {
  DVLOG(1) << __func__;

  // TODO(timav): Use DataPipe.
  client_->OnBufferDecoded(mojom::AudioBuffer::From(audio_buffer));
}

}  // namespace media
