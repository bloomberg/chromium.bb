// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_wrapper.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

namespace {

base::LazyInstance<MediaPipelineBackendManager> g_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MediaPipelineBackend* MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  return CreateMediaPipelineBackend(params, 0);
}

// static
MediaPipelineBackend* MediaPipelineBackendManager::CreateMediaPipelineBackend(
    const media::MediaPipelineDeviceParams& params,
    int stream_type) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineBackendManager* backend_manager = Get();
  MediaPipelineBackendWrapper* backend_ptr = new MediaPipelineBackendWrapper(
      media::CastMediaShlib::CreateMediaPipelineBackend(params), stream_type,
      backend_manager->GetVolumeMultiplier(stream_type));
  backend_manager->media_pipeline_backends_.push_back(backend_ptr);
  return backend_ptr;
}

// static
void MediaPipelineBackendManager::OnMediaPipelineBackendDestroyed(
    const MediaPipelineBackend* backend) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineBackendManager* backend_manager = Get();
  backend_manager->media_pipeline_backends_.erase(
      std::remove(backend_manager->media_pipeline_backends_.begin(),
                  backend_manager->media_pipeline_backends_.end(), backend),
      backend_manager->media_pipeline_backends_.end());
}

// static
void MediaPipelineBackendManager::SetVolumeMultiplier(int stream_type,
                                                      float volume) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineBackendManager* backend_manager = Get();
  volume = std::max(0.0f, std::min(volume, 1.0f));
  backend_manager->volume_by_stream_type_[stream_type] = volume;

  // Set volume for each open media pipeline backends.
  for (auto it = backend_manager->media_pipeline_backends_.begin();
       it != backend_manager->media_pipeline_backends_.end(); it++) {
    MediaPipelineBackendWrapper* wrapper =
        static_cast<MediaPipelineBackendWrapper*>(*it);
    if (wrapper->GetStreamType() == stream_type)
      wrapper->SetStreamTypeVolume(volume);
  }
}

// static
MediaPipelineBackendManager* MediaPipelineBackendManager::Get() {
  return g_instance.Pointer();
}

MediaPipelineBackendManager::MediaPipelineBackendManager() {
}

MediaPipelineBackendManager::~MediaPipelineBackendManager() {
}

float MediaPipelineBackendManager::GetVolumeMultiplier(int stream_type) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineBackendManager* backend_manager = Get();
  auto it = backend_manager->volume_by_stream_type_.find(stream_type);
  if (it == backend_manager->volume_by_stream_type_.end()) {
    return 1.0;
  } else {
    return it->second;
  }
}

}  // namespace media
}  // namespace chromecast
