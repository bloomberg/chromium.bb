// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

class DecoderBuffer;
class MediaKeys;

// A mojom::Decryptor implementation. This object is owned by the creator,
// and uses a weak binding across the mojo interface.
class MojoDecryptorService : public mojom::Decryptor {
 public:
  // Constructs a MojoDecryptorService and binds it to the |request|. Keeps a
  // copy of |cdm| to prevent it from being deleted as long as it is needed.
  // |error_handler| will be called if a connection error occurs.
  MojoDecryptorService(const scoped_refptr<MediaKeys>& cdm,
                       mojo::InterfaceRequest<mojom::Decryptor> request,
                       const mojo::Closure& error_handler);

  ~MojoDecryptorService() final;

  // mojom::Decryptor implementation.
  void Initialize(mojo::ScopedDataPipeConsumerHandle receive_pipe,
                  mojo::ScopedDataPipeProducerHandle transmit_pipe) final;
  void Decrypt(mojom::DemuxerStream::Type stream_type,
               mojom::DecoderBufferPtr encrypted,
               const DecryptCallback& callback) final;
  void CancelDecrypt(mojom::DemuxerStream::Type stream_type) final;
  void InitializeAudioDecoder(
      mojom::AudioDecoderConfigPtr config,
      const InitializeAudioDecoderCallback& callback) final;
  void InitializeVideoDecoder(
      mojom::VideoDecoderConfigPtr config,
      const InitializeVideoDecoderCallback& callback) final;
  void DecryptAndDecodeAudio(
      mojom::DecoderBufferPtr encrypted,
      const DecryptAndDecodeAudioCallback& callback) final;
  void DecryptAndDecodeVideo(
      mojom::DecoderBufferPtr encrypted,
      const DecryptAndDecodeVideoCallback& callback) final;
  void ResetDecoder(mojom::DemuxerStream::Type stream_type) final;
  void DeinitializeDecoder(mojom::DemuxerStream::Type stream_type) final;
  void ReleaseSharedBuffer(mojo::ScopedSharedBufferHandle buffer,
                           uint64_t buffer_size) final;

 private:
  // Callback executed once Decrypt() is done.
  void OnDecryptDone(const DecryptCallback& callback,
                     media::Decryptor::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);

  // Callbacks executed once decoder initialized.
  void OnAudioDecoderInitialized(const InitializeAudioDecoderCallback& callback,
                                 bool success);
  void OnVideoDecoderInitialized(const InitializeVideoDecoderCallback& callback,
                                 bool success);

  // Callbacks executed when DecryptAndDecode are done.
  void OnAudioDecoded(const DecryptAndDecodeAudioCallback& callback,
                      media::Decryptor::Status status,
                      const media::Decryptor::AudioFrames& frames);
  void OnVideoDecoded(const DecryptAndDecodeVideoCallback& callback,
                      media::Decryptor::Status status,
                      const scoped_refptr<VideoFrame>& frame);

  // Helper functions to write and read a DecoderBuffer.
  mojom::DecoderBufferPtr TransferDecoderBuffer(
      const scoped_refptr<DecoderBuffer>& buffer);
  scoped_refptr<DecoderBuffer> ReadDecoderBuffer(
      mojom::DecoderBufferPtr buffer);

  // A weak binding is used to connect to the MojoDecryptor.
  mojo::Binding<mojom::Decryptor> binding_;

  // DataPipes for serializing the data section of DecoderBuffer into/from.
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  // Keep ownership of |cdm_| while it is being used. |decryptor_| is the actual
  // Decryptor referenced by |cdm_|.
  scoped_refptr<MediaKeys> cdm_;
  media::Decryptor* decryptor_;

  // Keep a reference to VideoFrames until ReleaseSharedBuffer() is called
  // to release it.
  std::unordered_map<MojoHandle, scoped_refptr<VideoFrame>>
      in_use_video_frames_;

  base::WeakPtr<MojoDecryptorService> weak_this_;
  base::WeakPtrFactory<MojoDecryptorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptorService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
