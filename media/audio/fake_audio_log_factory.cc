// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_log_factory.h"

#include <memory>
#include <string>

namespace media {

class FakeAudioLogImpl : public AudioLog {
 public:
  FakeAudioLogImpl() = default;
  ~FakeAudioLogImpl() override = default;
  void OnCreated(int component_id,
                 const media::AudioParameters& params,
                 const std::string& device_id) override {}
  void OnStarted(int component_id) override {}
  void OnStopped(int component_id) override {}
  void OnClosed(int component_id) override {}
  void OnError(int component_id) override {}
  void OnSetVolume(int component_id, double volume) override {}
  void OnSwitchOutputDevice(int component_id,
                            const std::string& device_id) override {}
  void OnLogMessage(int component_id, const std::string& message) override {}
};

FakeAudioLogFactory::FakeAudioLogFactory() = default;
FakeAudioLogFactory::~FakeAudioLogFactory() = default;

std::unique_ptr<AudioLog> FakeAudioLogFactory::CreateAudioLog(
    AudioComponent component) {
  return std::make_unique<FakeAudioLogImpl>();
}

}  // namespace media
