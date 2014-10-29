// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/webmediaplayer_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/renderer.h"
#include "media/blink/null_encrypted_media_player_support.h"
#include "media/blink/webmediaplayer_impl.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/public/interfaces/application/shell.mojom.h"

namespace mojo {

WebMediaPlayerFactory::WebMediaPlayerFactory(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    bool enable_mojo_media_renderer)
    : compositor_task_runner_(compositor_task_runner),
      enable_mojo_media_renderer_(enable_mojo_media_renderer),
      media_thread_("Media"),
      audio_manager_(media::AudioManager::Create(&fake_audio_log_factory_)),
      audio_hardware_config_(
          audio_manager_->GetInputStreamParameters(
              media::AudioManagerBase::kDefaultDeviceId),
          audio_manager_->GetDefaultOutputStreamParameters()) {

  if (!media::IsMediaLibraryInitialized()) {
    base::FilePath module_dir;
    CHECK(PathService::Get(base::DIR_EXE, &module_dir));
    CHECK(media::InitializeMediaLibrary(module_dir));
  }
}

WebMediaPlayerFactory::~WebMediaPlayerFactory() {
}

blink::WebMediaPlayer* WebMediaPlayerFactory::CreateMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    Shell* shell) {
#if defined(OS_ANDROID)
  return NULL;
#else
  scoped_ptr<media::Renderer> renderer;

  if (enable_mojo_media_renderer_) {
    ServiceProviderPtr media_renderer_service_provider;
    shell->ConnectToApplication("mojo:mojo_media_renderer_app",
                                GetProxy(&media_renderer_service_provider));
    renderer.reset(new media::MojoRendererImpl(
        GetMediaThreadTaskRunner(), media_renderer_service_provider.get()));
  }

  media::WebMediaPlayerParams params(
      media::WebMediaPlayerParams::DeferLoadCB(),
      CreateAudioRendererSink(),
      GetAudioHardwareConfig(),
      new media::MediaLog(),
      scoped_refptr<media::GpuVideoAcceleratorFactories>(),
      GetMediaThreadTaskRunner(),
      compositor_task_runner_,
      base::Bind(&media::NullEncryptedMediaPlayerSupport::Create),
      NULL);
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate;

  return new media::WebMediaPlayerImpl(
      frame, client, delegate, renderer.Pass(), params);
#endif
}

const media::AudioHardwareConfig&
WebMediaPlayerFactory::GetAudioHardwareConfig() {
  return audio_hardware_config_;
}

scoped_refptr<media::AudioRendererSink>
WebMediaPlayerFactory::CreateAudioRendererSink() {
  // TODO(dalecurtis): Replace this with an interface to an actual mojo service;
  // the AudioOutputStreamSink will not work in sandboxed processes.
  return new media::AudioOutputStreamSink();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebMediaPlayerFactory::GetMediaThreadTaskRunner() {
  if (!media_thread_.IsRunning())
    media_thread_.Start();

  return media_thread_.message_loop_proxy();
}

}  // namespace mojo
