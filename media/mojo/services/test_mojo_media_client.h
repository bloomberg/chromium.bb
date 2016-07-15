// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager.h"
#include "media/mojo/services/mojo_media_client.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioRendererSink;
class MediaLog;
class RendererFactory;
class VideoRendererSink;

// Default MojoMediaClient for MojoMediaApplication.
class TestMojoMediaClient : public MojoMediaClient {
 public:
  TestMojoMediaClient();
  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  void WillQuit() final;
  std::unique_ptr<Renderer> CreateRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<MediaLog> media_log,
      const std::string& audio_device_id) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      shell::mojom::InterfaceProvider* /* interface_provider */) final;

 private:
  RendererFactory* GetRendererFactory(scoped_refptr<MediaLog> media_log);
  AudioRendererSink* GetAudioRendererSink();
  VideoRendererSink* GetVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  ScopedAudioManagerPtr audio_manager_;
  std::unique_ptr<RendererFactory> renderer_factory_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  std::unique_ptr<VideoRendererSink> video_renderer_sink_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
