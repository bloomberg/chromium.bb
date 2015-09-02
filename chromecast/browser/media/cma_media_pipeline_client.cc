// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cma_media_pipeline_client.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

CmaMediaPipelineClient::CmaMediaPipelineClient() : client_(nullptr) {}

CmaMediaPipelineClient::~CmaMediaPipelineClient() {}

scoped_ptr<MediaPipelineBackend>
CmaMediaPipelineClient::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  return make_scoped_ptr(CastMediaShlib::CreateMediaPipelineBackend(params));
}

void CmaMediaPipelineClient::OnMediaPipelineBackendCreated() {
  if (client_)
    client_->OnResourceAcquired(this);
}

void CmaMediaPipelineClient::OnMediaPipelineBackendDestroyed() {
  if (client_)
    client_->OnResourceReleased(this, CastResource::kResourceNone);
}

void CmaMediaPipelineClient::ReleaseResource(CastResource::Resource resource) {
  CastResource::Resource audio_video_resource =
      static_cast<CastResource::Resource>(CastResource::kResourceAudio |
                                          CastResource::kResourceScreenPrimary);

  // TODO(yucliu): media pipeline need to stop audio video seperately
  if (!(resource & audio_video_resource)) {
    if (client_)
      client_->OnResourceReleased(this, audio_video_resource);
  }
}

void CmaMediaPipelineClient::SetCastResourceClient(
    CastResource::Client* client) {
  client_ = client;
}

}  // namespace media
}  // namespace chromecast
