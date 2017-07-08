// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

class DecoderBuffer;
class MojoDecoderBufferReader;
class MojoDecoderBufferWriter;

// A mojom::Decryptor implementation. This object is owned by the creator,
// and uses a weak binding across the mojo interface.
class MEDIA_MOJO_EXPORT MojoDecryptorService
    : NON_EXPORTED_BASE(public mojom::Decryptor) {
 public:
  using StreamType = media::Decryptor::StreamType;
  using Status = media::Decryptor::Status;

  // Constructs a MojoDecryptorService and binds it to the |request|.
  // |error_handler| will be called if a connection error occurs.
  // Caller must ensure that |decryptor| outlives |this|.
  MojoDecryptorService(media::Decryptor* decryptor,
                       mojo::InterfaceRequest<mojom::Decryptor> request,
                       const base::Closure& error_handler);

  ~MojoDecryptorService() final;

  // mojom::Decryptor implementation.
  void Initialize(mojo::ScopedDataPipeConsumerHandle receive_pipe,
                  mojo::ScopedDataPipeProducerHandle transmit_pipe) final;
  void Decrypt(StreamType stream_type,
               mojom::DecoderBufferPtr encrypted,
               const DecryptCallback& callback) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(
      const AudioDecoderConfig& config,
      const InitializeAudioDecoderCallback& callback) final;
  void InitializeVideoDecoder(
      const VideoDecoderConfig& config,
      const InitializeVideoDecoderCallback& callback) final;
  void DecryptAndDecodeAudio(
      mojom::DecoderBufferPtr encrypted,
      const DecryptAndDecodeAudioCallback& callback) final;
  void DecryptAndDecodeVideo(
      mojom::DecoderBufferPtr encrypted,
      const DecryptAndDecodeVideoCallback& callback) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

 private:
  void OnReadDone(StreamType stream_type,
                  const DecryptCallback& callback,
                  scoped_refptr<DecoderBuffer> buffer);

  // Callback executed once Decrypt() is done.
  void OnDecryptDone(const DecryptCallback& callback,
                     Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);

  // Callbacks executed once decoder initialized.
  void OnAudioDecoderInitialized(const InitializeAudioDecoderCallback& callback,
                                 bool success);
  void OnVideoDecoderInitialized(const InitializeVideoDecoderCallback& callback,
                                 bool success);

  void OnAudioRead(const DecryptAndDecodeAudioCallback& callback,
                   scoped_refptr<DecoderBuffer> buffer);
  void OnVideoRead(const DecryptAndDecodeVideoCallback& callback,
                   scoped_refptr<DecoderBuffer> buffer);

  // Callbacks executed when DecryptAndDecode are done.
  void OnAudioDecoded(const DecryptAndDecodeAudioCallback& callback,
                      Status status,
                      const media::Decryptor::AudioFrames& frames);
  void OnVideoDecoded(const DecryptAndDecodeVideoCallback& callback,
                      Status status,
                      const scoped_refptr<VideoFrame>& frame);

  // A weak binding is used to connect to the MojoDecryptor.
  mojo::Binding<mojom::Decryptor> binding_;

  // Helper class to send decrypted DecoderBuffer to the client.
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  // Helper class to receive encrypted DecoderBuffer from the client.
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  media::Decryptor* decryptor_;

  base::WeakPtr<MojoDecryptorService> weak_this_;
  base::WeakPtrFactory<MojoDecryptorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecryptorService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECRYPTOR_SERVICE_H_
