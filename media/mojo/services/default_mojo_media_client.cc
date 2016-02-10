// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/base/null_video_sink.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {

namespace {
class DefaultMojoMediaClient : public MojoMediaClient {
 public:
  DefaultMojoMediaClient() {}

  // MojoMediaClient overrides.
  void Initialize() override {
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

  AudioRendererSink* CreateAudioRendererSink() override {
    if (!audio_renderer_sink_)
      audio_renderer_sink_ = new AudioOutputStreamSink();

    return audio_renderer_sink_.get();
  }

  VideoRendererSink* CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) override {
    if (!video_renderer_sink_) {
      video_renderer_sink_ = make_scoped_ptr(
          new NullVideoSink(false, base::TimeDelta::FromSecondsD(1.0 / 60),
                            NullVideoSink::NewFrameCB(), task_runner));
    }

    return video_renderer_sink_.get();
  }

  scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::InterfaceProvider* /* service_provider */) override {
    return make_scoped_ptr(new DefaultCdmFactory());
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<AudioHardwareConfig> audio_hardware_config_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_ptr<VideoRendererSink> video_renderer_sink_;

  DISALLOW_COPY_AND_ASSIGN(DefaultMojoMediaClient);
};

}  // namespace (anonymous)

scoped_ptr<MojoMediaClient> MojoMediaClient::Create() {
  return make_scoped_ptr(new DefaultMojoMediaClient());
}

}  // namespace media
