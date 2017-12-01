// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/services/mojo_media_client.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioManager;
class AudioRendererSink;
class MediaLog;
class RendererFactory;
class VideoRendererSink;

// Default MojoMediaClient for MediaService.
class TestMojoMediaClient : public MojoMediaClient {
 public:
  TestMojoMediaClient();
  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize(service_manager::Connector* connector) final;
  scoped_refptr<AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id) final;
  std::unique_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) final;
  std::unique_ptr<RendererFactory> CreateRendererFactory(
      MediaLog* media_log) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* /* host_interfaces */) final;

 private:
  std::unique_ptr<AudioManager> audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
