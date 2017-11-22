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
    MojoCdmServiceContext* mojo_cdm_service_context,
    std::unique_ptr<media::AudioDecoder> decoder)
    : mojo_cdm_service_context_(mojo_cdm_service_context),
      decoder_(std::move(decoder)),
      weak_factory_(this) {
  DCHECK(mojo_cdm_service_context_);
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
                                         InitializeCallback callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();

  // Get CdmContext from cdm_id if the stream is encrypted.
  CdmContext* cdm_context = nullptr;
  scoped_refptr<ContentDecryptionModule> cdm;
  if (config.is_encrypted()) {
    cdm = mojo_cdm_service_context_->GetCdm(cdm_id);
    if (!cdm) {
      DVLOG(1) << "CDM not found for CDM id: " << cdm_id;
      std::move(callback).Run(false, false);
      return;
    }

    cdm_context = cdm->GetCdmContext();
    if (!cdm_context) {
      DVLOG(1) << "CDM context not available for CDM id: " << cdm_id;
      std::move(callback).Run(false, false);
      return;
    }
  }

  decoder_->Initialize(
      config, cdm_context,
      base::Bind(&MojoAudioDecoderService::OnInitialized, weak_this_,
                 base::Passed(&callback), cdm),
      base::Bind(&MojoAudioDecoderService::OnAudioBufferReady, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __func__;

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(receive_pipe)));
}

void MojoAudioDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     DecodeCallback callback) {
  DVLOG(3) << __func__;
  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoAudioDecoderService::OnReadDone,
                                        weak_this_, std::move(callback)));
}

void MojoAudioDecoderService::Reset(ResetCallback callback) {
  DVLOG(1) << __func__;
  decoder_->Reset(base::Bind(&MojoAudioDecoderService::OnResetDone, weak_this_,
                             base::Passed(&callback)));
}

void MojoAudioDecoderService::OnInitialized(
    InitializeCallback callback,
    scoped_refptr<ContentDecryptionModule> cdm,
    bool success) {
  DVLOG(1) << __func__ << " success:" << success;

  if (success) {
    cdm_ = cdm;
    std::move(callback).Run(success, decoder_->NeedsBitstreamConversion());
  } else {
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    std::move(callback).Run(false, false);
  }
}

// The following methods are needed so that we can bind them with a weak pointer
// to avoid running the |callback| after connection error happens and |this| is
// deleted. It's not safe to run the |callback| after a connection error.

void MojoAudioDecoderService::OnReadDone(DecodeCallback callback,
                                         scoped_refptr<DecoderBuffer> buffer) {
  DVLOG(3) << __func__ << " success:" << !!buffer;

  if (!buffer) {
    std::move(callback).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(buffer, base::Bind(&MojoAudioDecoderService::OnDecodeStatus,
                                      weak_this_, base::Passed(&callback)));
}

void MojoAudioDecoderService::OnDecodeStatus(DecodeCallback callback,
                                             media::DecodeStatus status) {
  DVLOG(3) << __func__ << " status:" << status;
  std::move(callback).Run(status);
}

void MojoAudioDecoderService::OnResetDone(ResetCallback callback) {
  DVLOG(1) << __func__;
  std::move(callback).Run();
}

void MojoAudioDecoderService::OnAudioBufferReady(
    const scoped_refptr<AudioBuffer>& audio_buffer) {
  DVLOG(1) << __func__;

  // TODO(timav): Use DataPipe.
  client_->OnBufferDecoded(mojom::AudioBuffer::From(audio_buffer));
}

}  // namespace media
