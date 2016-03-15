// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decoder_factory.h"

#include "media/mojo/services/mojo_audio_decoder.h"
#include "media/mojo/services/mojo_video_decoder.h"

namespace media {

MojoDecoderFactory::MojoDecoderFactory(
    mojo::shell::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
}

MojoDecoderFactory::~MojoDecoderFactory() {}

void MojoDecoderFactory::CreateAudioDecoders(
    ScopedVector<AudioDecoder>* audio_decoders) {
#if defined(ENABLE_MOJO_AUDIO_DECODER)
  // TODO(xhwang): Connect to mojo audio decoder service and pass it here.
  audio_decoders->push_back(new media::MojoAudioDecoder());
#endif
}

void MojoDecoderFactory::CreateVideoDecoders(
    ScopedVector<VideoDecoder>* video_decoders) {
#if defined(ENABLE_MOJO_VIDEO_DECODER)
  // TODO(sandersd): Connect to mojo video decoder service and pass it here.
  video_decoders->push_back(new media::MojoVideoDecoder());
#endif
}

}  // namespace media
