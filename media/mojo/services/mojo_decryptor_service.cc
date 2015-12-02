// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor_service.h"

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "media/mojo/services/media_type_converters.h"

namespace media {

MojoDecryptorService::MojoDecryptorService(
    const scoped_refptr<MediaKeys>& cdm,
    mojo::InterfaceRequest<interfaces::Decryptor> request,
    const mojo::Closure& error_handler)
    : binding_(this, request.Pass()), cdm_(cdm), weak_factory_(this) {
  decryptor_ = cdm->GetCdmContext()->GetDecryptor();
  DCHECK(decryptor_);
  weak_this_ = weak_factory_.GetWeakPtr();
  binding_.set_connection_error_handler(error_handler);
}

MojoDecryptorService::~MojoDecryptorService() {}

void MojoDecryptorService::Decrypt(interfaces::DemuxerStream::Type stream_type,
                                   interfaces::DecoderBufferPtr encrypted,
                                   const DecryptCallback& callback) {
  decryptor_->Decrypt(
      static_cast<media::Decryptor::StreamType>(stream_type),
      encrypted.To<scoped_refptr<DecoderBuffer>>(),
      base::Bind(&MojoDecryptorService::OnDecryptDone, weak_this_, callback));
}

void MojoDecryptorService::CancelDecrypt(
    interfaces::DemuxerStream::Type stream_type) {
  decryptor_->CancelDecrypt(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::InitializeAudioDecoder(
    interfaces::AudioDecoderConfigPtr config,
    const InitializeAudioDecoderCallback& callback) {
  decryptor_->InitializeAudioDecoder(
      config.To<AudioDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnAudioDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::InitializeVideoDecoder(
    interfaces::VideoDecoderConfigPtr config,
    const InitializeVideoDecoderCallback& callback) {
  decryptor_->InitializeVideoDecoder(
      config.To<VideoDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnVideoDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::DecryptAndDecodeAudio(
    interfaces::DecoderBufferPtr encrypted,
    const DecryptAndDecodeAudioCallback& callback) {
  decryptor_->DecryptAndDecodeAudio(
      encrypted.To<scoped_refptr<DecoderBuffer>>(),
      base::Bind(&MojoDecryptorService::OnAudioDecoded, weak_this_, callback));
}

void MojoDecryptorService::DecryptAndDecodeVideo(
    interfaces::DecoderBufferPtr encrypted,
    const DecryptAndDecodeVideoCallback& callback) {
  decryptor_->DecryptAndDecodeVideo(
      encrypted.To<scoped_refptr<DecoderBuffer>>(),
      base::Bind(&MojoDecryptorService::OnVideoDecoded, weak_this_, callback));
}

void MojoDecryptorService::ResetDecoder(
    interfaces::DemuxerStream::Type stream_type) {
  decryptor_->ResetDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::DeinitializeDecoder(
    interfaces::DemuxerStream::Type stream_type) {
  decryptor_->DeinitializeDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::OnDecryptDone(
    const DecryptCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(1) << __FUNCTION__ << "(" << status << ")";
  callback.Run(static_cast<Decryptor::Status>(status),
               interfaces::DecoderBuffer::From(buffer));
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
  DVLOG(1) << __FUNCTION__ << "(" << status << ")";

  mojo::Array<interfaces::AudioBufferPtr> audio_buffers;
  for (const auto& frame : frames)
    audio_buffers.push_back(interfaces::AudioBuffer::From(frame).Pass());

  callback.Run(static_cast<Decryptor::Status>(status), audio_buffers.Pass());
}

void MojoDecryptorService::OnVideoDecoded(
    const DecryptAndDecodeVideoCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG(1) << __FUNCTION__ << "(" << status << ")";

  callback.Run(static_cast<Decryptor::Status>(status),
               interfaces::VideoFrame::From(frame).Pass());
}

}  // namespace media
