// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_MEDIA_FACTORY_H_
#define COMPONENTS_HTML_VIEWER_MEDIA_FACTORY_H_

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/audio_hardware_config.h"
#include "media/blink/url_index.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebEncryptedMediaClient;
class WebMediaPlayer;
class WebLocalFrame;
class WebURL;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}

namespace media {
class AudioManager;
class RestartableAudioRendererSink;
class CdmFactory;
class MediaPermission;
class UrlIndex;
class WebEncryptedMediaClientImpl;
}

namespace mojo {
class ServiceProvider;
namespace shell {
namespace mojom {
class Shell;
}
}
}

namespace html_viewer {

// Helper class used to create blink::WebMediaPlayer objects.
// This class stores the "global state" shared across all WebMediaPlayer
// instances.
class MediaFactory {
 public:
  MediaFactory(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      mojo::shell::mojom::Shell* shell);
  ~MediaFactory();

  blink::WebMediaPlayer* CreateMediaPlayer(
      blink::WebLocalFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      mojo::shell::mojom::Shell* shell);

  blink::WebEncryptedMediaClient* GetEncryptedMediaClient();

 private:
  media::interfaces::ServiceFactory* GetMediaServiceFactory();
  media::MediaPermission* GetMediaPermission();
  media::CdmFactory* GetCdmFactory();

#if !defined(OS_ANDROID)
  const media::AudioHardwareConfig& GetAudioHardwareConfig();
  scoped_refptr<media::RestartableAudioRendererSink> CreateAudioRendererSink();
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaThreadTaskRunner();

  base::Thread media_thread_;
  media::FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<media::AudioManager> audio_manager_;
  media::AudioHardwareConfig audio_hardware_config_;
#endif

  const bool enable_mojo_media_renderer_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  mojo::shell::mojom::Shell* shell_;

  // Lazily initialized objects.
  media::interfaces::ServiceFactoryPtr media_service_factory_;
  scoped_ptr<media::WebEncryptedMediaClientImpl> web_encrypted_media_client_;
  scoped_ptr<media::MediaPermission> media_permission_;
  scoped_ptr<media::CdmFactory> cdm_factory_;

  // Media resource cache, lazily initialized.
  linked_ptr<media::UrlIndex> url_index_;

  DISALLOW_COPY_AND_ASSIGN(MediaFactory);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_MEDIA_FACTORY_H_
