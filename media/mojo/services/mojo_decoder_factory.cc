// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decoder_factory.h"

#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_audio_decoder.h"

namespace media {

MojoDecoderFactory::MojoDecoderFactory(
    interfaces::ServiceFactory* service_factory)
    : service_factory_(service_factory) {
  DCHECK(service_factory_);
}

MojoDecoderFactory::~MojoDecoderFactory() {}

void MojoDecoderFactory::CreateAudioDecoders(
    ScopedVector<AudioDecoder>* audio_decoders) {
  // TODO(xhwang): Connect to mojo audio decoder service and pass it here.
  audio_decoders->push_back(new media::MojoAudioDecoder());
}

}  // namespace media
