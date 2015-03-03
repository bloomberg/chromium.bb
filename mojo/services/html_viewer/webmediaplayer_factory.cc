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
#include "media/blink/webmediaplayer_impl.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_renderer_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/shell.mojom.h"

using mojo::ServiceProviderPtr;

namespace html_viewer {

#if !defined(OS_ANDROID)
namespace {

class RendererServiceProvider
    : public media::MojoRendererFactory::ServiceProvider {
 public:
  explicit RendererServiceProvider(ServiceProviderPtr service_provider_ptr)
      : service_provider_ptr_(service_provider_ptr.Pass()) {}
  ~RendererServiceProvider() final {}

  void ConnectToService(
      mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) final {
    mojo::ConnectToService(service_provider_ptr_.get(), media_renderer_ptr);
  }

 private:
  ServiceProviderPtr service_provider_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RendererServiceProvider);
};

}  // namespace
#endif

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
    media::MediaPermission* media_permission,
    blink::WebContentDecryptionModule* initial_cdm,
    mojo::Shell* shell) {
#if defined(OS_ANDROID)
  return nullptr;
#else
  scoped_refptr<media::MediaLog> media_log(new media::MediaLog());
  scoped_ptr<media::RendererFactory> media_renderer_factory;

  if (enable_mojo_media_renderer_) {
    ServiceProviderPtr media_renderer_service_provider;
    shell->ConnectToApplication(
        "mojo:media", GetProxy(&media_renderer_service_provider), nullptr);
    media_renderer_factory.reset(new media::MojoRendererFactory(make_scoped_ptr(
        new RendererServiceProvider(media_renderer_service_provider.Pass()))));
  } else {
    media_renderer_factory.reset(
        new media::DefaultRendererFactory(media_log,
                                          nullptr,  // No GPU factory.
                                          GetAudioHardwareConfig()));
  }

  media::WebMediaPlayerParams params(
      media::WebMediaPlayerParams::DeferLoadCB(), CreateAudioRendererSink(),
      media_log, GetMediaThreadTaskRunner(), compositor_task_runner_,
      media::WebMediaPlayerParams::Context3DCB(), media_permission,
      initial_cdm);
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate;

  scoped_ptr<media::CdmFactory> cdm_factory(new media::DefaultCdmFactory());

  return new media::WebMediaPlayerImpl(frame, client, delegate,
                                       media_renderer_factory.Pass(),
                                       cdm_factory.Pass(), params);
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

}  // namespace html_viewer
