// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/mojo/services/mojo_audio_decoder_service.h"

namespace media {

MojoAudioDecoderService::MojoAudioDecoderService(
    scoped_ptr<AudioDecoder> decoder,
    mojo::InterfaceRequest<interfaces::AudioDecoder> request)
    : binding_(this, std::move(request)), decoder_(std::move(decoder)) {}

MojoAudioDecoderService::~MojoAudioDecoderService() {}

void MojoAudioDecoderService::Initialize(
    interfaces::AudioDecoderClientPtr client,
    interfaces::AudioDecoderConfigPtr config,
    int32_t cdm_id,
    const InitializeCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(false, false);
}

void MojoAudioDecoderService::Decode(interfaces::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(DecodeStatus::DECODE_ERROR);
}

void MojoAudioDecoderService::Reset(const ResetCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run();
}

}  // namespace media
