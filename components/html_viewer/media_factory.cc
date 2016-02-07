// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/media_factory.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
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
#include "media/mojo/interfaces/renderer.mojom.h"
#include "media/mojo/services/mojo_cdm_factory.h"
#include "media/mojo/services/mojo_renderer_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/shell/public/cpp/shell.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace html_viewer {

namespace {

// Enable mojo media Renderer in media pipeline instead of using the internal
// media::Renderer implementation.
// TODO(xhwang): Move this to media_switches.h.
const char kEnableMojoMediaRenderer[] = "enable-mojo-media-renderer";

bool AreSecureCodecsSupported() {
  // Hardware-secure codecs are not currently supported by HTML Viewer on any
  // platform.
  return false;
}

}  // namespace

MediaFactory::MediaFactory(
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    mojo::Shell* shell)
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
      compositor_task_runner_(compositor_task_runner),
      shell_(shell) {
  media::InitializeMediaLibrary();
}

MediaFactory::~MediaFactory() {
}

blink::WebMediaPlayer* MediaFactory::CreateMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm,
    mojo::Shell* shell) {
#if defined(OS_ANDROID)
  // TODO(xhwang): Get CreateMediaPlayer working on android.
  return nullptr;
#else
  scoped_refptr<media::MediaLog> media_log(new media::MediaLog());
  scoped_ptr<media::RendererFactory> media_renderer_factory;

  if (enable_mojo_media_renderer_) {
    media_renderer_factory.reset(
        new media::MojoRendererFactory(GetMediaServiceFactory()));
  } else {
    media_renderer_factory.reset(
        new media::DefaultRendererFactory(media_log,
                                          nullptr,  // No GPU factory.
                                          GetAudioHardwareConfig()));
  }

  media::WebMediaPlayerParams params(
      media::WebMediaPlayerParams::DeferLoadCB(), CreateAudioRendererSink(),
      media_log, GetMediaThreadTaskRunner(), GetMediaThreadTaskRunner(),
      compositor_task_runner_, media::WebMediaPlayerParams::Context3DCB(),
      base::Bind(&v8::Isolate::AdjustAmountOfExternalAllocatedMemory,
                 base::Unretained(blink::mainThreadIsolate())),
      GetMediaPermission(), initial_cdm);
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate;

  if (!url_index_.get() || url_index_->frame() != frame) {
    url_index_.reset(new media::UrlIndex(frame));
  }

  return new media::WebMediaPlayerImpl(
      frame, client, encrypted_client, delegate,
      std::move(media_renderer_factory), GetCdmFactory(), url_index_, params);
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

media::interfaces::ServiceFactory* MediaFactory::GetMediaServiceFactory() {
  if (!media_service_factory_)
    shell_->ConnectToService("mojo:media", &media_service_factory_);
  return media_service_factory_.get();
}

media::MediaPermission* MediaFactory::GetMediaPermission() {
  // TODO(xhwang): Replace DefaultMediaPermission with something real when
  // permissions are supported in html_viewer.
  NOTIMPLEMENTED();

  if (!media_permission_)
    media_permission_.reset(new media::DefaultMediaPermission(true));
  return media_permission_.get();
}

media::CdmFactory* MediaFactory::GetCdmFactory() {
  if (!cdm_factory_) {
    if (enable_mojo_media_renderer_)
      cdm_factory_.reset(new media::MojoCdmFactory(GetMediaServiceFactory()));
    else
      cdm_factory_.reset(new media::DefaultCdmFactory());
  }

  return cdm_factory_.get();
}

#if !defined(OS_ANDROID)
const media::AudioHardwareConfig& MediaFactory::GetAudioHardwareConfig() {
  return audio_hardware_config_;
}

scoped_refptr<media::RestartableAudioRendererSink>
MediaFactory::CreateAudioRendererSink() {
  // TODO(dalecurtis): Replace this with an interface to an actual mojo service;
  // the AudioOutputStreamSink will not work in sandboxed processes.
  return new media::AudioOutputStreamSink();
}

scoped_refptr<base::SingleThreadTaskRunner>
MediaFactory::GetMediaThreadTaskRunner() {
  if (!media_thread_.IsRunning())
    media_thread_.Start();

  return media_thread_.task_runner();
}
#endif  // !defined(OS_ANDROID)

}  // namespace html_viewer
