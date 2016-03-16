// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {

class MojoAudioDecoderService : public interfaces::AudioDecoder {
 public:
  MojoAudioDecoderService(
      scoped_ptr<AudioDecoder> decoder,
      mojo::InterfaceRequest<interfaces::AudioDecoder> request);

  ~MojoAudioDecoderService() final;

  // interfaces::AudioDecoder implementation
  void Initialize(interfaces::AudioDecoderClientPtr client,
                  interfaces::AudioDecoderConfigPtr config,
                  int32_t cdm_id,
                  const InitializeCallback& callback) final;

  void Decode(interfaces::DecoderBufferPtr buffer,
              const DecodeCallback& callback) final;

  void Reset(const ResetCallback& callback) final;

 private:
  // A binding represents the association between the service and the
  // communication channel, i.e. the pipe.
  mojo::StrongBinding<interfaces::AudioDecoder> binding_;

  // The AudioDecoder that does actual decoding work.
  scoped_ptr<AudioDecoder> decoder_;

  // The destination for the decoded buffers.
  interfaces::AudioDecoderClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoderService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
