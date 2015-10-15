// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cma_media_pipeline_client.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

CmaMediaPipelineClient::CmaMediaPipelineClient() : media_pipeline_count_(0) {
  thread_checker_.DetachFromThread();
}

CmaMediaPipelineClient::~CmaMediaPipelineClient() {}

scoped_ptr<MediaPipelineBackend>
CmaMediaPipelineClient::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  return make_scoped_ptr(CastMediaShlib::CreateMediaPipelineBackend(params));
}

void CmaMediaPipelineClient::OnMediaPipelineBackendCreated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_count_++;

  if (media_pipeline_count_ == 1)
    NotifyResourceAcquired();
}

void CmaMediaPipelineClient::OnMediaPipelineBackendDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_count_--;

  if (media_pipeline_count_ == 0)
    NotifyResourceReleased(CastResource::kResourceNone);
}

void CmaMediaPipelineClient::ReleaseResource(CastResource::Resource resource) {
  CastResource::Resource audio_video_resource =
      static_cast<CastResource::Resource>(CastResource::kResourceAudio |
                                          CastResource::kResourceScreenPrimary);

  // TODO(yucliu): media pipeline need to stop audio video seperately
  if (!(resource & audio_video_resource))
    NotifyResourceReleased(audio_video_resource);
}

}  // namespace media
}  // namespace chromecast
