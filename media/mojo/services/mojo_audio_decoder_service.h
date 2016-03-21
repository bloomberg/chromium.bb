// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

class MojoAudioDecoderService : public interfaces::AudioDecoder {
 public:
  MojoAudioDecoderService(
      scoped_ptr<media::AudioDecoder> decoder,
      mojo::InterfaceRequest<interfaces::AudioDecoder> request);

  ~MojoAudioDecoderService() final;

  // interfaces::AudioDecoder implementation
  void Initialize(interfaces::AudioDecoderClientPtr client,
                  interfaces::AudioDecoderConfigPtr config,
                  int32_t cdm_id,
                  const InitializeCallback& callback) final;

  void SetDataSource(mojo::ScopedDataPipeConsumerHandle receive_pipe) final;

  void Decode(interfaces::DecoderBufferPtr buffer,
              const DecodeCallback& callback) final;

  void Reset(const ResetCallback& callback) final;

 private:
  // Called by |decoder_| upon finishing initialization.
  void OnInitialized(const InitializeCallback& callback, bool success);

  // Called by |decoder_| when DecoderBuffer is accepted or rejected.
  void OnDecodeStatus(const DecodeCallback& callback,
                      media::AudioDecoder::Status status);

  // Called by |decoder_| when reset sequence is finished.
  void OnResetDone(const ResetCallback& callback);

  // Called by |decoder_| for each decoded buffer.
  void OnAudioBufferReady(const scoped_refptr<AudioBuffer>& audio_buffer);

  // A helper method to read and deserialize DecoderBuffer from data pipe.
  scoped_refptr<DecoderBuffer> ReadDecoderBuffer(
      interfaces::DecoderBufferPtr buffer);

  // A binding represents the association between the service and the
  // communication channel, i.e. the pipe.
  mojo::StrongBinding<interfaces::AudioDecoder> binding_;

  // DataPipe for serializing the data section of DecoderBuffer.
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  // The AudioDecoder that does actual decoding work.
  scoped_ptr<media::AudioDecoder> decoder_;

  // The destination for the decoded buffers.
  interfaces::AudioDecoderClientPtr client_;

  base::WeakPtr<MojoAudioDecoderService> weak_this_;
  base::WeakPtrFactory<MojoAudioDecoderService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoderService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
