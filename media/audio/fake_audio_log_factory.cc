// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_log_factory.h"

namespace media {

class FakeAudioLogImpl : public AudioLog {
 public:
  FakeAudioLogImpl() {}
  virtual ~FakeAudioLogImpl() {}
  virtual void OnCreated(int component_id,
                         const media::AudioParameters& params,
                         const std::string& device_id) override {}
  virtual void OnStarted(int component_id) override {}
  virtual void OnStopped(int component_id) override {}
  virtual void OnClosed(int component_id) override {}
  virtual void OnError(int component_id) override {}
  virtual void OnSetVolume(int component_id, double volume) override {}
};

FakeAudioLogFactory::FakeAudioLogFactory() {}
FakeAudioLogFactory::~FakeAudioLogFactory() {}

scoped_ptr<AudioLog> FakeAudioLogFactory::CreateAudioLog(
    AudioComponent component) {
  return scoped_ptr<AudioLog>(new FakeAudioLogImpl());
}

}  // namespace media
