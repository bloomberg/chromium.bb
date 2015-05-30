// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/media_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/blink/webencryptedmediaclient_impl.h"
#include "media/blink/webmediaplayer_impl.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/filters/default_media_permission.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_renderer_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"

using mojo::ServiceProviderPtr;

namespace html_viewer {

namespace {

// Enable MediaRenderer in media pipeline instead of using the internal
// media::Renderer implementation.
// TODO(xhwang): Move this to media_switches.h.
const char kEnableMojoMediaRenderer[] = "enable-mojo-media-renderer";

#if !defined(OS_ANDROID)
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
#endif

bool AreSecureCodecsSupported() {
  // Hardware-secure codecs are not currently supported by HTML Viewer on any
  // platform.
  return false;
}

}  // namespace

MediaFactory::MediaFactory(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner)
    :
#if !defined(OS_ANDROID)
      media_thread_("Media"),
      audio_manager_(media::AudioManager::Create(&fake_audio_log_factory_)),
      audio_hardware_config_(
          audio_manager_->GetInputStreamParameters(
              media::AudioManagerBase::kDefaultDeviceId),
          audio_manager_->GetDefaultOutputStreamParameters()),
#endif
      enable_mojo_media_renderer_(base::CommandLine::ForCurrentProcess()
                                      ->HasSwitch(kEnableMojoMediaRenderer)),
      compositor_task_runner_(compositor_task_runner) {
  if (!media::IsMediaLibraryInitialized()) {
    base::FilePath module_dir;
    CHECK(PathService::Get(base::DIR_EXE, &module_dir));
    CHECK(media::InitializeMediaLibrary(module_dir));
  }
}

MediaFactory::~MediaFactory() {
}

blink::WebMediaPlayer* MediaFactory::CreateMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebContentDecryptionModule* initial_cdm,
    mojo::Shell* shell) {
#if defined(OS_ANDROID)
  // TODO(xhwang): Get CreateMediaPlayer working on android.
  return nullptr;
#else
  scoped_refptr<media::MediaLog> media_log(new media::MediaLog());
  scoped_ptr<media::RendererFactory> media_renderer_factory;

  if (enable_mojo_media_renderer_) {
    ServiceProviderPtr media_renderer_service_provider;
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:media");
    shell->ConnectToApplication(
        request.Pass(), GetProxy(&media_renderer_service_provider), nullptr);
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
      media::WebMediaPlayerParams::Context3DCB(), GetMediaPermission(),
      initial_cdm);
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate;

  return new media::WebMediaPlayerImpl(frame, client, delegate,
                                       media_renderer_factory.Pass(),
                                       GetCdmFactory(), params);
#endif  // defined(OS_ANDROID)
}

blink::WebEncryptedMediaClient* MediaFactory::GetEncryptedMediaClient() {
  if (!web_encrypted_media_client_) {
    web_encrypted_media_client_.reset(new media::WebEncryptedMediaClientImpl(
        base::Bind(&AreSecureCodecsSupported), GetCdmFactory(),
        GetMediaPermission()));
  }
  return web_encrypted_media_client_.get();
}

media::MediaPermission* MediaFactory::GetMediaPermission() {
  if (!media_permission_)
    media_permission_.reset(new media::DefaultMediaPermission(true));
  return media_permission_.get();
}

media::CdmFactory* MediaFactory::GetCdmFactory() {
  if (!cdm_factory_)
    cdm_factory_.reset(new media::DefaultCdmFactory());
  return cdm_factory_.get();
}

#if !defined(OS_ANDROID)
const media::AudioHardwareConfig& MediaFactory::GetAudioHardwareConfig() {
  return audio_hardware_config_;
}

scoped_refptr<media::AudioRendererSink>
MediaFactory::CreateAudioRendererSink() {
  // TODO(dalecurtis): Replace this with an interface to an actual mojo service;
  // the AudioOutputStreamSink will not work in sandboxed processes.
  return new media::AudioOutputStreamSink();
}

scoped_refptr<base::SingleThreadTaskRunner>
MediaFactory::GetMediaThreadTaskRunner() {
  if (!media_thread_.IsRunning())
    media_thread_.Start();

  return media_thread_.message_loop_proxy();
}
#endif  // !defined(OS_ANDROID)

}  // namespace html_viewer
