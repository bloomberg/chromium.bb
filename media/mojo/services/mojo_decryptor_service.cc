// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"

namespace media {

MojoDecryptorService::MojoDecryptorService(
    const scoped_refptr<MediaKeys>& cdm,
    mojo::InterfaceRequest<mojom::Decryptor> request,
    const mojo::Closure& error_handler)
    : binding_(this, std::move(request)), cdm_(cdm), weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  decryptor_ = cdm->GetCdmContext()->GetDecryptor();
  DCHECK(decryptor_);
  weak_this_ = weak_factory_.GetWeakPtr();
  binding_.set_connection_error_handler(error_handler);
}

MojoDecryptorService::~MojoDecryptorService() {}

void MojoDecryptorService::Initialize(
    mojo::ScopedDataPipeConsumerHandle receive_pipe,
    mojo::ScopedDataPipeProducerHandle transmit_pipe) {
  producer_handle_ = std::move(transmit_pipe);
  consumer_handle_ = std::move(receive_pipe);
}

void MojoDecryptorService::Decrypt(mojom::DemuxerStream::Type stream_type,
                                   mojom::DecoderBufferPtr encrypted,
                                   const DecryptCallback& callback) {
  DVLOG(3) << __FUNCTION__;
  decryptor_->Decrypt(
      static_cast<media::Decryptor::StreamType>(stream_type),
      ReadDecoderBuffer(std::move(encrypted)),
      base::Bind(&MojoDecryptorService::OnDecryptDone, weak_this_, callback));
}

void MojoDecryptorService::CancelDecrypt(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->CancelDecrypt(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::InitializeAudioDecoder(
    mojom::AudioDecoderConfigPtr config,
    const InitializeAudioDecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->InitializeAudioDecoder(
      config.To<AudioDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnAudioDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::InitializeVideoDecoder(
    mojom::VideoDecoderConfigPtr config,
    const InitializeVideoDecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->InitializeVideoDecoder(
      config.To<VideoDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnVideoDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::DecryptAndDecodeAudio(
    mojom::DecoderBufferPtr encrypted,
    const DecryptAndDecodeAudioCallback& callback) {
  DVLOG(3) << __FUNCTION__;
  decryptor_->DecryptAndDecodeAudio(
      ReadDecoderBuffer(std::move(encrypted)),
      base::Bind(&MojoDecryptorService::OnAudioDecoded, weak_this_, callback));
}

void MojoDecryptorService::DecryptAndDecodeVideo(
    mojom::DecoderBufferPtr encrypted,
    const DecryptAndDecodeVideoCallback& callback) {
  DVLOG(3) << __FUNCTION__;
  decryptor_->DecryptAndDecodeVideo(
      ReadDecoderBuffer(std::move(encrypted)),
      base::Bind(&MojoDecryptorService::OnVideoDecoded, weak_this_, callback));
}

void MojoDecryptorService::ResetDecoder(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->ResetDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::DeinitializeDecoder(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->DeinitializeDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::ReleaseSharedBuffer(
    mojo::ScopedSharedBufferHandle buffer,
    uint64_t buffer_size) {
  in_use_video_frames_.erase(buffer.get().value());
}

void MojoDecryptorService::OnDecryptDone(
    const DecryptCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  if (!buffer) {
    DCHECK_NE(status, media::Decryptor::kSuccess);
    callback.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  callback.Run(static_cast<Decryptor::Status>(status),
               TransferDecoderBuffer(buffer));
}

void MojoDecryptorService::OnAudioDecoderInitialized(
    const InitializeAudioDecoderCallback& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";
  callback.Run(success);
}

void MojoDecryptorService::OnVideoDecoderInitialized(
    const InitializeVideoDecoderCallback& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";
  callback.Run(success);
}

void MojoDecryptorService::OnAudioDecoded(
    const DecryptAndDecodeAudioCallback& callback,
    media::Decryptor::Status status,
    const media::Decryptor::AudioFrames& frames) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  mojo::Array<mojom::AudioBufferPtr> audio_buffers;
  for (const auto& frame : frames)
    audio_buffers.push_back(mojom::AudioBuffer::From(frame));

  callback.Run(static_cast<Decryptor::Status>(status),
               std::move(audio_buffers));
}

void MojoDecryptorService::OnVideoDecoded(
    const DecryptAndDecodeVideoCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  if (!frame) {
    DCHECK_NE(status, media::Decryptor::kSuccess);
    callback.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  // If |frame| has shared memory that will be passed back, keep a reference
  // to it until the other side is done with the memory.
  if (frame->storage_type() == VideoFrame::STORAGE_MOJO_SHARED_BUFFER) {
    MojoSharedBufferVideoFrame* mojo_frame =
        static_cast<MojoSharedBufferVideoFrame*>(frame.get());
    in_use_video_frames_.insert(
        std::make_pair(mojo_frame->Handle().value(), frame));
  }

  callback.Run(static_cast<Decryptor::Status>(status),
               mojom::VideoFrame::From(frame));
}

mojom::DecoderBufferPtr MojoDecryptorService::TransferDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& encrypted) {
  mojom::DecoderBufferPtr buffer = mojom::DecoderBuffer::From(encrypted);
  if (encrypted->end_of_stream())
    return buffer;

  // Serialize the data section of the DecoderBuffer into our pipe.
  uint32_t bytes_to_write =
      base::checked_cast<uint32_t>(encrypted->data_size());
  DCHECK_GT(bytes_to_write, 0u);
  uint32_t bytes_written = bytes_to_write;
  CHECK_EQ(WriteDataRaw(producer_handle_.get(), encrypted->data(),
                        &bytes_written, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(bytes_to_write, bytes_written);
  return buffer;
}

scoped_refptr<DecoderBuffer> MojoDecryptorService::ReadDecoderBuffer(
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
  CHECK_EQ(MOJO_HANDLE_SIGNAL_READABLE, state.satisfied_signals);

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
