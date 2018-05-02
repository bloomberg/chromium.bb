// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_broker.h"

#include <utility>

#include "content/browser/media/audio_output_stream_broker.h"

namespace content {

namespace {

class AudioStreamBrokerFactoryImpl final : public AudioStreamBrokerFactory {
 public:
  AudioStreamBrokerFactoryImpl() = default;
  ~AudioStreamBrokerFactoryImpl() final = default;

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      media::mojom::AudioOutputStreamProviderClientPtr client) final {
    return std::make_unique<AudioOutputStreamBroker>(
        render_process_id, render_frame_id, stream_id, output_device_id, params,
        group_id, std::move(deleter), std::move(client));
  }
};

}  // namespace

AudioStreamBroker::AudioStreamBroker(int render_process_id, int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}
AudioStreamBroker::~AudioStreamBroker() {}

AudioStreamBrokerFactory::AudioStreamBrokerFactory() {}
AudioStreamBrokerFactory::~AudioStreamBrokerFactory() {}

// static
std::unique_ptr<AudioStreamBrokerFactory>
AudioStreamBrokerFactory::CreateImpl() {
  return std::make_unique<AudioStreamBrokerFactoryImpl>();
}

}  // namespace content
