// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_

#include "chromecast/media/service/media_pipeline_backend_factory.h"
#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class MediaResourceTracker;
class VideoResolutionPolicy;

class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  using CreateCdmFactoryCB =
      base::Callback<std::unique_ptr<::media::CdmFactory>()>;

  CastMojoMediaClient(const CreateMediaPipelineBackendCB& create_backend_cb,
                      const CreateCdmFactoryCB& create_cdm_factory_cb,
                      VideoResolutionPolicy* video_resolution_policy,
                      MediaResourceTracker* media_resource_tracker);
  ~CastMojoMediaClient() override;

  // MojoMediaClient overrides.
  scoped_refptr<::media::AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id) override;
  std::unique_ptr<::media::RendererFactory> CreateRendererFactory(
      const scoped_refptr<::media::MediaLog>& media_log) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* interface_provider) override;

 private:
  const CreateMediaPipelineBackendCB create_backend_cb_;
  const CreateCdmFactoryCB create_cdm_factory_cb_;
  VideoResolutionPolicy* video_resolution_policy_;
  MediaResourceTracker* media_resource_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
