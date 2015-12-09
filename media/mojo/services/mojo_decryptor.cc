// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor.h"

#include <utility>

#include "base/bind.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "mojo/application/public/cpp/connect.h"

namespace media {

MojoDecryptor::MojoDecryptor(interfaces::DecryptorPtr remote_decryptor)
    : remote_decryptor_(std::move(remote_decryptor)), weak_factory_(this) {}

MojoDecryptor::~MojoDecryptor() {}

void MojoDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                     const NewKeyCB& key_added_cb) {
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = key_added_cb;
      break;
    case kVideo:
      new_video_key_cb_ = key_added_cb;
      break;
    default:
      NOTREACHED();
  }
}

void MojoDecryptor::Decrypt(StreamType stream_type,
                            const scoped_refptr<DecoderBuffer>& encrypted,
                            const DecryptCB& decrypt_cb) {
  remote_decryptor_->Decrypt(
      static_cast<interfaces::DemuxerStream::Type>(stream_type),
      interfaces::DecoderBuffer::From(encrypted),
      base::Bind(&MojoDecryptor::OnBufferDecrypted, weak_factory_.GetWeakPtr(),
                 decrypt_cb));
}

void MojoDecryptor::CancelDecrypt(StreamType stream_type) {
  remote_decryptor_->CancelDecrypt(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  remote_decryptor_->InitializeAudioDecoder(
      interfaces::AudioDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  remote_decryptor_->InitializeVideoDecoder(
      interfaces::VideoDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  remote_decryptor_->DecryptAndDecodeAudio(
      interfaces::DecoderBuffer::From(encrypted),
      base::Bind(&MojoDecryptor::OnAudioDecoded, weak_factory_.GetWeakPtr(),
                 audio_decode_cb));
}

void MojoDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  remote_decryptor_->DecryptAndDecodeVideo(
      interfaces::DecoderBuffer::From(encrypted),
      base::Bind(&MojoDecryptor::OnVideoDecoded, weak_factory_.GetWeakPtr(),
                 video_decode_cb));
}

void MojoDecryptor::ResetDecoder(StreamType stream_type) {
  remote_decryptor_->ResetDecoder(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::DeinitializeDecoder(StreamType stream_type) {
  remote_decryptor_->DeinitializeDecoder(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::OnKeyAdded() {
  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void MojoDecryptor::OnBufferDecrypted(const DecryptCB& decrypt_cb,
                                      interfaces::Decryptor::Status status,
                                      interfaces::DecoderBufferPtr buffer) {
  decrypt_cb.Run(static_cast<Decryptor::Status>(status),
                 std::move(buffer.To<scoped_refptr<DecoderBuffer>>()));
}

void MojoDecryptor::OnAudioDecoded(
    const AudioDecodeCB& audio_decode_cb,
    interfaces::Decryptor::Status status,
    mojo::Array<interfaces::AudioBufferPtr> audio_buffers) {
  Decryptor::AudioFrames audio_frames;
  for (size_t i = 0; i < audio_buffers.size(); ++i)
    audio_frames.push_back(audio_buffers[i].To<scoped_refptr<AudioBuffer>>());

  audio_decode_cb.Run(static_cast<Decryptor::Status>(status), audio_frames);
}

void MojoDecryptor::OnVideoDecoded(const VideoDecodeCB& video_decode_cb,
                                   interfaces::Decryptor::Status status,
                                   interfaces::VideoFramePtr video_frame) {
  video_decode_cb.Run(static_cast<Decryptor::Status>(status),
                      std::move(video_frame.To<scoped_refptr<VideoFrame>>()));
}

}  // namespace media
