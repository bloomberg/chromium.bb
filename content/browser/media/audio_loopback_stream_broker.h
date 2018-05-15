// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "content/browser/media/audio_muting_session.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace content {

// AudioLoopbackStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It is operated on the UI thread.
class CONTENT_EXPORT AudioLoopbackStreamBroker final
    : public AudioStreamBroker,
      public media::mojom::AudioInputStreamObserver {
 public:
  AudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<AudioStreamBrokerFactory::LoopbackSource> source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client);

  ~AudioLoopbackStreamBroker() final;

  // Creates the stream.
  void CreateStream(audio::mojom::StreamFactory* factory) final;

  // media::AudioInputStreamObserver implementation.
  void DidStartRecording() final;

 private:
  void StreamCreated(media::mojom::AudioInputStreamPtr stream,
                     media::mojom::AudioDataPipePtr data_pipe);
  void Cleanup();

  const std::unique_ptr<AudioStreamBrokerFactory::LoopbackSource> source_;
  const media::AudioParameters params_;
  const uint32_t shared_memory_count_;

  DeleterCallback deleter_;

  // Constructed only if the loopback source playback should be muted while the
  // loopback stream is running.
  base::Optional<AudioMutingSession> muter_;

  mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client_;
  mojo::Binding<AudioInputStreamObserver> observer_binding_;
  media::mojom::AudioInputStreamClientRequest client_request_;

  base::WeakPtrFactory<AudioLoopbackStreamBroker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioLoopbackStreamBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_
