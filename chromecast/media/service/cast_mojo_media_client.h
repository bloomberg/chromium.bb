// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_

#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class MediaPipelineBackendFactory;
class MediaResourceTracker;
class VideoModeSwitcher;
class VideoResolutionPolicy;

class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  using CreateCdmFactoryCB =
      base::Callback<std::unique_ptr<::media::CdmFactory>()>;

  CastMojoMediaClient(MediaPipelineBackendFactory* backend_factory,
                      const CreateCdmFactoryCB& create_cdm_factory_cb,
                      VideoModeSwitcher* video_mode_switcher,
                      VideoResolutionPolicy* video_resolution_policy,
                      MediaResourceTracker* media_resource_tracker);
  ~CastMojoMediaClient() override;

  // MojoMediaClient overrides.
  void Initialize(service_manager::Connector* connector) override;
  scoped_refptr<::media::AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id) override;
  std::unique_ptr<::media::RendererFactory> CreateRendererFactory(
      const scoped_refptr<::media::MediaLog>& media_log) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override;

 private:
  service_manager::Connector* connector_;
  MediaPipelineBackendFactory* const backend_factory_;
  const CreateCdmFactoryCB create_cdm_factory_cb_;
  VideoModeSwitcher* video_mode_switcher_;
  VideoResolutionPolicy* video_resolution_policy_;
  MediaResourceTracker* media_resource_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
