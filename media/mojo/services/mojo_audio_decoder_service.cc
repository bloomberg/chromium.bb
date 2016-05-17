// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/cdm_context.h"
#include "media/base/media_keys.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

static mojom::AudioDecoder::DecodeStatus ConvertDecodeStatus(
    media::DecodeStatus status) {
  switch (status) {
    case media::DecodeStatus::OK:
      return mojom::AudioDecoder::DecodeStatus::OK;
    case media::DecodeStatus::ABORTED:
      return mojom::AudioDecoder::DecodeStatus::ABORTED;
    case media::DecodeStatus::DECODE_ERROR:
      return mojom::AudioDecoder::DecodeStatus::DECODE_ERROR;
  }
  NOTREACHED();
  return mojom::AudioDecoder::DecodeStatus::DECODE_ERROR;
}

MojoAudioDecoderService::MojoAudioDecoderService(
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
    std::unique_ptr<media::AudioDecoder> decoder,
    mojo::InterfaceRequest<mojom::AudioDecoder> request)
    : binding_(this, std::move(request)),
      mojo_cdm_service_context_(mojo_cdm_service_context),
      decoder_(std::move(decoder)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioDecoderService::~MojoAudioDecoderService() {}

void MojoAudioDecoderService::Initialize(mojom::AudioDecoderClientPtr client,
                                         mojom::AudioDecoderConfigPtr config,
                                         int32_t cdm_id,
                                         const InitializeCallback& callback) {
  DVLOG(1) << __FUNCTION__ << " "
           << config.To<media::AudioDecoderConfig>().AsHumanReadableString();

  // Get CdmContext from cdm_id if the stream is encrypted.
  CdmContext* cdm_context = nullptr;
  scoped_refptr<MediaKeys> cdm;
  if (config.To<media::AudioDecoderConfig>().is_encrypted()) {
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

  client_ = std::move(client);

  decoder_->Initialize(
      config.To<media::AudioDecoderConfig>(), cdm_context,
      base::Bind(&MojoAudioDecoderService::OnInitialized, weak_this_, callback,
                 cdm),
      base::Bind(&MojoAudioDecoderService::OnAudioBufferReady, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __FUNCTION__;
  consumer_handle_ = std::move(receive_pipe);
}

void MojoAudioDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  DVLOG(3) << __FUNCTION__;

  scoped_refptr<DecoderBuffer> media_buffer =
      ReadDecoderBuffer(std::move(buffer));
  if (!media_buffer) {
    callback.Run(ConvertDecodeStatus(media::DecodeStatus::DECODE_ERROR));
    return;
  }

  decoder_->Decode(media_buffer,
                   base::Bind(&MojoAudioDecoderService::OnDecodeStatus,
                              weak_this_, callback));
}

void MojoAudioDecoderService::Reset(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decoder_->Reset(
      base::Bind(&MojoAudioDecoderService::OnResetDone, weak_this_, callback));
}

void MojoAudioDecoderService::OnInitialized(const InitializeCallback& callback,
                                            scoped_refptr<MediaKeys> cdm,
                                            bool success) {
  DVLOG(1) << __FUNCTION__ << " success:" << success;

  if (success) {
    cdm_ = cdm;
    callback.Run(success, decoder_->NeedsBitstreamConversion());
  } else {
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    callback.Run(false, false);
  }
}

void MojoAudioDecoderService::OnDecodeStatus(const DecodeCallback& callback,
                                             media::DecodeStatus status) {
  DVLOG(3) << __FUNCTION__ << " status:" << status;
  callback.Run(ConvertDecodeStatus(status));
}

void MojoAudioDecoderService::OnResetDone(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  callback.Run();
}

void MojoAudioDecoderService::OnAudioBufferReady(
    const scoped_refptr<AudioBuffer>& audio_buffer) {
  DVLOG(1) << __FUNCTION__;

  // TODO(timav): Use DataPipe.
  client_->OnBufferDecoded(mojom::AudioBuffer::From(audio_buffer));
}

scoped_refptr<DecoderBuffer> MojoAudioDecoderService::ReadDecoderBuffer(
    mojom::DecoderBufferPtr buffer) {
  scoped_refptr<DecoderBuffer> media_buffer(
      buffer.To<scoped_refptr<DecoderBuffer>>());

  if (media_buffer->end_of_stream())
    return media_buffer;

  // Wait for the data to become available in the DataPipe.
  MojoHandleSignalsState state;
  CHECK_EQ(MOJO_RESULT_OK,
           MojoWait(consumer_handle_.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, &state));

  if (state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED) {
    DVLOG(1) << __FUNCTION__ << ": Peer closed the data pipe";
    return scoped_refptr<DecoderBuffer>();
  }

  CHECK_EQ(MOJO_HANDLE_SIGNAL_READABLE,
           state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);

  // Read the inner data for the DecoderBuffer from our DataPipe.
  uint32_t bytes_to_read =
      base::checked_cast<uint32_t>(media_buffer->data_size());
  DCHECK_GT(bytes_to_read, 0u);
  uint32_t bytes_read = bytes_to_read;
  CHECK_EQ(ReadDataRaw(consumer_handle_.get(), media_buffer->writable_data(),
                       &bytes_read, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(bytes_to_read, bytes_read);

  return media_buffer;
}

}  // namespace media
