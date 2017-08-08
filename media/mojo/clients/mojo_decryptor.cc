// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_decryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/scoped_callback_runner.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {

namespace {

void ReleaseFrameResource(mojom::FrameResourceReleaserPtr releaser) {
  // Close the connection, which will result in the service realizing the frame
  // resource is no longer needed.
  releaser.reset();
}

}  // namespace

MojoDecryptor::MojoDecryptor(mojom::DecryptorPtr remote_decryptor)
    : remote_decryptor_(std::move(remote_decryptor)), weak_factory_(this) {
  DVLOG(1) << __func__;

  // Allocate DataPipe size based on video content.

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      DemuxerStream::VIDEO, &remote_consumer_handle);

  mojo::ScopedDataPipeProducerHandle remote_producer_handle;
  mojo_decoder_buffer_reader_ = MojoDecoderBufferReader::Create(
      DemuxerStream::VIDEO, &remote_producer_handle);

  // Pass the other end of each pipe to |remote_decryptor_|.
  remote_decryptor_->Initialize(std::move(remote_consumer_handle),
                                std::move(remote_producer_handle));
}

MojoDecryptor::~MojoDecryptor() {
  DVLOG(1) << __func__;
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
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  remote_decryptor_->Decrypt(
      stream_type, std::move(mojo_buffer),
      base::BindOnce(
          &MojoDecryptor::OnBufferDecrypted, weak_factory_.GetWeakPtr(),
          ScopedCallbackRunner(ToOnceCallback(decrypt_cb), kError, nullptr)));
}

void MojoDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->CancelDecrypt(stream_type);
}

void MojoDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeAudioDecoder(
      config, ScopedCallbackRunner(ToOnceCallback(init_cb), false));
}

void MojoDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeVideoDecoder(
      config, ScopedCallbackRunner(ToOnceCallback(init_cb), false));
}

void MojoDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    audio_decode_cb.Run(kError, AudioFrames());
    return;
  }

  remote_decryptor_->DecryptAndDecodeAudio(
      std::move(mojo_buffer),
      base::BindOnce(&MojoDecryptor::OnAudioDecoded, weak_factory_.GetWeakPtr(),
                     ScopedCallbackRunner(ToOnceCallback(audio_decode_cb),
                                          kError, AudioFrames())));
}

void MojoDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    video_decode_cb.Run(kError, nullptr);
    return;
  }

  remote_decryptor_->DecryptAndDecodeVideo(
      std::move(mojo_buffer),
      base::BindOnce(&MojoDecryptor::OnVideoDecoded, weak_factory_.GetWeakPtr(),
                     ScopedCallbackRunner(ToOnceCallback(video_decode_cb),
                                          kError, nullptr)));
}

void MojoDecryptor::ResetDecoder(StreamType stream_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->ResetDecoder(stream_type);
}

void MojoDecryptor::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DeinitializeDecoder(stream_type);
}

void MojoDecryptor::OnKeyAdded() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void MojoDecryptor::OnBufferDecrypted(DecryptOnceCB decrypt_cb,
                                      Status status,
                                      mojom::DecoderBufferPtr buffer) {
  DVLOG_IF(1, status != kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer.is_null()) {
    std::move(decrypt_cb).Run(status, nullptr);
    return;
  }

  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer),
      base::BindOnce(&MojoDecryptor::OnBufferRead, weak_factory_.GetWeakPtr(),
                     std::move(decrypt_cb), status));
}

void MojoDecryptor::OnBufferRead(DecryptOnceCB decrypt_cb,
                                 Status status,
                                 scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(decrypt_cb).Run(kError, nullptr);
    return;
  }

  std::move(decrypt_cb).Run(status, buffer);
}

void MojoDecryptor::OnAudioDecoded(
    AudioDecodeOnceCB audio_decode_cb,
    Status status,
    std::vector<mojom::AudioBufferPtr> audio_buffers) {
  DVLOG_IF(1, status != kSuccess) << __func__ << "(" << status << ")";
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  Decryptor::AudioFrames audio_frames;
  for (size_t i = 0; i < audio_buffers.size(); ++i)
    audio_frames.push_back(audio_buffers[i].To<scoped_refptr<AudioBuffer>>());

  std::move(audio_decode_cb).Run(status, audio_frames);
}

void MojoDecryptor::OnVideoDecoded(VideoDecodeOnceCB video_decode_cb,
                                   Status status,
                                   const scoped_refptr<VideoFrame>& video_frame,
                                   mojom::FrameResourceReleaserPtr releaser) {
  DVLOG_IF(1, status != kSuccess) << __func__ << ": status = " << status;
  DVLOG_IF(3, status == kSuccess) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // If using shared memory, ensure that |releaser| is closed when
  // |frame| is destroyed.
  if (video_frame && releaser) {
    video_frame->AddDestructionObserver(
        base::Bind(&ReleaseFrameResource, base::Passed(&releaser)));
  }

  std::move(video_decode_cb).Run(status, video_frame);
}

}  // namespace media
