// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_export.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class AudioHardwareConfig;
class AudioRendererSink;
class VideoRendererSink;

// Default MojoMediaClient for MojoMediaApplication.
class MEDIA_EXPORT DefaultMojoMediaClient : public MojoMediaClient {
 public:
  DefaultMojoMediaClient();
  ~DefaultMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  scoped_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log) final;
  AudioRendererSink* CreateAudioRendererSink() final;
  VideoRendererSink* CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) final;
  scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::shell::mojom::InterfaceProvider* /* service_provider */) final;

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<AudioHardwareConfig> audio_hardware_config_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_ptr<VideoRendererSink> video_renderer_sink_;

  DISALLOW_COPY_AND_ASSIGN(DefaultMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_DEFAULT_MOJO_MEDIA_CLIENT_H_
