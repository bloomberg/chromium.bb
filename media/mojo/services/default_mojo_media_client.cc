// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media.h"
#include "media/base/null_video_sink.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {
namespace internal {

class DefaultMojoMediaClient : public PlatformMojoMediaClient {
 public:
  DefaultMojoMediaClient() {
    InitializeMediaLibrary();

    // TODO(dalecurtis): We should find a single owner per process for the audio
    // manager or make it a lazy instance.  It's not safe to call Get()/Create()
    // across multiple threads...
    //
    // TODO(dalecurtis): Eventually we'll want something other than a fake audio
    // log factory here too.  We should probably at least DVLOG() such info.
    AudioManager* audio_manager = AudioManager::Get();
    if (!audio_manager)
      audio_manager = media::AudioManager::Create(&fake_audio_log_factory_);

    audio_hardware_config_.reset(new AudioHardwareConfig(
        audio_manager->GetInputStreamParameters(
            AudioManagerBase::kDefaultDeviceId),
        audio_manager->GetDefaultOutputStreamParameters()));
  }

  scoped_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log) override {
    return make_scoped_ptr(new DefaultRendererFactory(media_log, nullptr,
                                                      *audio_hardware_config_));
  }

  scoped_refptr<AudioRendererSink> CreateAudioRendererSink() override {
    return new AudioOutputStreamSink();
  }

  scoped_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) override {
    return make_scoped_ptr(
        new NullVideoSink(false, base::TimeDelta::FromSecondsD(1.0 / 60),
                          NullVideoSink::NewFrameCB(), task_runner));
  }

  const AudioHardwareConfig* GetAudioHardwareConfig() override {
    return audio_hardware_config_.get();
  }

  scoped_ptr<CdmFactory> CreateCdmFactory() override {
    return make_scoped_ptr(new DefaultCdmFactory());
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<AudioHardwareConfig> audio_hardware_config_;

  DISALLOW_COPY_AND_ASSIGN(DefaultMojoMediaClient);
};

scoped_ptr<PlatformMojoMediaClient> CreatePlatformMojoMediaClient() {
  return make_scoped_ptr(new DefaultMojoMediaClient());
}

}  // namespace internal
}  // namespace media
