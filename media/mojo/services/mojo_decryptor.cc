// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "mojo/shell/public/cpp/connect.h"

namespace media {

MojoDecryptor::MojoDecryptor(interfaces::DecryptorPtr remote_decryptor)
    : remote_decryptor_(std::move(remote_decryptor)), weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  CreateDataPipes();
}

MojoDecryptor::~MojoDecryptor() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                     const NewKeyCB& key_added_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->Decrypt(
      static_cast<interfaces::DemuxerStream::Type>(stream_type),
      TransferDecoderBuffer(encrypted),
      base::Bind(&MojoDecryptor::OnBufferDecrypted, weak_factory_.GetWeakPtr(),
                 decrypt_cb));
}

void MojoDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->CancelDecrypt(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeAudioDecoder(
      interfaces::AudioDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeVideoDecoder(
      interfaces::VideoDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DecryptAndDecodeAudio(
      TransferDecoderBuffer(encrypted),
      base::Bind(&MojoDecryptor::OnAudioDecoded, weak_factory_.GetWeakPtr(),
                 audio_decode_cb));
}

void MojoDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DecryptAndDecodeVideo(
      TransferDecoderBuffer(encrypted),
      base::Bind(&MojoDecryptor::OnVideoDecoded, weak_factory_.GetWeakPtr(),
                 video_decode_cb));
}

void MojoDecryptor::ResetDecoder(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->ResetDecoder(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DeinitializeDecoder(
      static_cast<interfaces::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void MojoDecryptor::OnBufferDecrypted(const DecryptCB& decrypt_cb,
                                      interfaces::Decryptor::Status status,
                                      interfaces::DecoderBufferPtr buffer) {
  DVLOG_IF(1, status != interfaces::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == interfaces::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer.is_null()) {
    decrypt_cb.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  decrypt_cb.Run(static_cast<Decryptor::Status>(status),
                 ReadDecoderBuffer(std::move(buffer)));
}

void MojoDecryptor::OnAudioDecoded(
    const AudioDecodeCB& audio_decode_cb,
    interfaces::Decryptor::Status status,
    mojo::Array<interfaces::AudioBufferPtr> audio_buffers) {
  DVLOG_IF(1, status != interfaces::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == interfaces::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  Decryptor::AudioFrames audio_frames;
  for (size_t i = 0; i < audio_buffers.size(); ++i)
    audio_frames.push_back(audio_buffers[i].To<scoped_refptr<AudioBuffer>>());

  audio_decode_cb.Run(static_cast<Decryptor::Status>(status), audio_frames);
}

void MojoDecryptor::OnVideoDecoded(const VideoDecodeCB& video_decode_cb,
                                   interfaces::Decryptor::Status status,
                                   interfaces::VideoFramePtr video_frame) {
  DVLOG_IF(1, status != interfaces::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == interfaces::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame.is_null()) {
    video_decode_cb.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  scoped_refptr<VideoFrame> frame(video_frame.To<scoped_refptr<VideoFrame>>());
  video_decode_cb.Run(static_cast<Decryptor::Status>(status), frame);
}

void MojoDecryptor::CreateDataPipes() {
  // Allocate DataPipe size based on video content. Video can get quite large;
  // at 4K, VP9 delivers packets which are ~1MB in size; so allow for 50%
  // headroom.
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 1.5 * (1024 * 1024);

  // Create 2 pipes, one for each direction.
  mojo::DataPipe write_pipe(options);
  mojo::DataPipe read_pipe(options);

  // Keep one end of each pipe.
  producer_handle_ = std::move(write_pipe.producer_handle);
  consumer_handle_ = std::move(read_pipe.consumer_handle);

  // Pass the other end of each pipe to |remote_decryptor_|.
  remote_decryptor_->Initialize(std::move(write_pipe.consumer_handle),
                                std::move(read_pipe.producer_handle));
}

interfaces::DecoderBufferPtr MojoDecryptor::TransferDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& encrypted) {
  interfaces::DecoderBufferPtr buffer =
      interfaces::DecoderBuffer::From(encrypted);
  if (encrypted->end_of_stream())
    return buffer;

  // Serialize the data section of the DecoderBuffer into our pipe.
  uint32_t num_bytes = base::checked_cast<uint32_t>(encrypted->data_size());
  DCHECK_GT(num_bytes, 0u);
  CHECK_EQ(WriteDataRaw(producer_handle_.get(), encrypted->data(), &num_bytes,
                        MOJO_READ_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(encrypted->data_size()));
  return buffer;
}

scoped_refptr<DecoderBuffer> MojoDecryptor::ReadDecoderBuffer(
    interfaces::DecoderBufferPtr buffer) {
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
  uint32_t num_bytes = base::checked_cast<uint32_t>(media_buffer->data_size());
  DCHECK_GT(num_bytes, 0u);
  CHECK_EQ(ReadDataRaw(consumer_handle_.get(), media_buffer->writable_data(),
                       &num_bytes, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(media_buffer->data_size()));
  return media_buffer;
}

}  // namespace media
