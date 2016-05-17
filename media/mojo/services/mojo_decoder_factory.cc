// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decoder_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"
#include "media/mojo/services/mojo_audio_decoder.h"
#include "media/mojo/services/mojo_video_decoder.h"
#include "services/shell/public/cpp/connect.h"

namespace media {

MojoDecoderFactory::MojoDecoderFactory(
    shell::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
}

MojoDecoderFactory::~MojoDecoderFactory() {}

void MojoDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ScopedVector<AudioDecoder>* audio_decoders) {
#if defined(ENABLE_MOJO_AUDIO_DECODER)
  mojom::AudioDecoderPtr audio_decoder_ptr;
  shell::GetInterface<mojom::AudioDecoder>(interface_provider_,
                                           &audio_decoder_ptr);

  audio_decoders->push_back(
      new media::MojoAudioDecoder(task_runner, std::move(audio_decoder_ptr)));
#endif
}

void MojoDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ScopedVector<VideoDecoder>* video_decoders) {
#if defined(ENABLE_MOJO_VIDEO_DECODER)
  // TODO(sandersd): Connect to mojo video decoder service and pass it here.
  video_decoders->push_back(new media::MojoVideoDecoder());
#endif
}

}  // namespace media
