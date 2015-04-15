// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/renderer_config.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media.h"
#include "media/filters/opus_audio_decoder.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {
namespace internal {

class DefaultRendererConfig : public PlatformRendererConfig {
 public:
  DefaultRendererConfig() {
    // TODO(dalecurtis): This will not work if the process is sandboxed...
    if (!media::IsMediaLibraryInitialized()) {
      base::FilePath module_dir;
      CHECK(PathService::Get(base::DIR_EXE, &module_dir));
      CHECK(media::InitializeMediaLibrary(module_dir));
    }

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

  ScopedVector<AudioDecoder> GetAudioDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb) override {
    ScopedVector<AudioDecoder> audio_decoders;

#if !defined(MEDIA_DISABLE_FFMPEG)
    audio_decoders.push_back(
        new FFmpegAudioDecoder(media_task_runner, media_log_cb));
    audio_decoders.push_back(new OpusAudioDecoder(media_task_runner));
#endif

    return audio_decoders.Pass();
  }

  ScopedVector<VideoDecoder> GetVideoDecoders(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const LogCB& media_log_cb) override {
    ScopedVector<VideoDecoder> video_decoders;

    // TODO(dalecurtis): If we ever need GPU video decoders, we'll need to
    // figure out how to retrieve the GpuVideoAcceleratorFactories...

#if !defined(MEDIA_DISABLE_LIBVPX)
    video_decoders.push_back(new VpxVideoDecoder(media_task_runner));
#endif

#if !defined(MEDIA_DISABLE_FFMPEG)
    video_decoders.push_back(new FFmpegVideoDecoder(media_task_runner));
#endif

    return video_decoders.Pass();
  }

  scoped_refptr<AudioRendererSink> GetAudioRendererSink() override {
    return new AudioOutputStreamSink();
  }

  const AudioHardwareConfig& GetAudioHardwareConfig() override {
    return *audio_hardware_config_;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<AudioHardwareConfig> audio_hardware_config_;

  DISALLOW_COPY_AND_ASSIGN(DefaultRendererConfig);
};

scoped_ptr<PlatformRendererConfig> CreatePlatformRendererConfig() {
  return make_scoped_ptr(new DefaultRendererConfig());
}

}  // namespace internal
}  // namespace media
