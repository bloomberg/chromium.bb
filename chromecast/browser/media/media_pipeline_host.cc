// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_pipeline_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/common/media/shared_memory_chunk.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device_params.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc_streamer/coded_frame_provider_host.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"

namespace chromecast {
namespace media {

struct MediaPipelineHost::MediaTrackHost {
  MediaTrackHost();
  ~MediaTrackHost();

  base::Closure pipe_write_cb;
};

MediaPipelineHost::MediaTrackHost::MediaTrackHost() {
}

MediaPipelineHost::MediaTrackHost::~MediaTrackHost() {
}

MediaPipelineHost::MediaPipelineHost() {
  thread_checker_.DetachFromThread();
}

MediaPipelineHost::~MediaPipelineHost() {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (MediaTrackMap::iterator it = media_track_map_.begin();
       it != media_track_map_.end(); ++it) {
    scoped_ptr<MediaTrackHost> media_track(it->second);
  }
  media_track_map_.clear();
}

void MediaPipelineHost::Initialize(
    LoadType load_type,
    const MediaPipelineClient& client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_.reset(new MediaPipelineImpl());
  MediaPipelineDeviceParams default_parameters;
  if (load_type == kLoadTypeMediaStream)
    default_parameters.sync_type = MediaPipelineDeviceParams::kModeIgnorePts;
  media_pipeline_->Initialize(
      load_type,
      CreateMediaPipelineDevice(default_parameters).Pass());
  media_pipeline_->SetClient(client);
}

void MediaPipelineHost::SetAvPipe(
    TrackId track_id,
    scoped_ptr<base::SharedMemory> shared_mem,
    const base::Closure& pipe_read_activity_cb,
    const base::Closure& av_pipe_set_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId || track_id == kVideoTrackId);

  size_t shared_mem_size = shared_mem->requested_size();
  scoped_ptr<MediaMemoryChunk> shared_memory_chunk(
      new SharedMemoryChunk(shared_mem.Pass(), shared_mem_size));
  scoped_ptr<MediaMessageFifo> media_message_fifo(
      new MediaMessageFifo(shared_memory_chunk.Pass(), shared_mem_size));
  media_message_fifo->ObserveReadActivity(pipe_read_activity_cb);
  scoped_ptr<CodedFrameProviderHost> frame_provider_host(
      new CodedFrameProviderHost(media_message_fifo.Pass()));

  MediaTrackMap::iterator it = media_track_map_.find(track_id);
  MediaTrackHost* media_track_host;
  if (it == media_track_map_.end()) {
    media_track_host = new MediaTrackHost();
    media_track_map_.insert(
        std::pair<TrackId, MediaTrackHost*>(track_id, media_track_host));
  } else {
    media_track_host = it->second;
  }
  media_track_host->pipe_write_cb = frame_provider_host->GetFifoWriteEventCb();

  scoped_ptr<CodedFrameProvider> frame_provider(frame_provider_host.release());
  if (track_id == kAudioTrackId) {
    media_pipeline_->GetAudioPipelineImpl()->SetCodedFrameProvider(
        frame_provider.Pass());
  } else {
    media_pipeline_->GetVideoPipelineImpl()->SetCodedFrameProvider(
        frame_provider.Pass());
  }
  av_pipe_set_cb.Run();
}

void MediaPipelineHost::AudioInitialize(
    TrackId track_id,
    const AvPipelineClient& client,
    const ::media::AudioDecoderConfig& config,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId);
  media_pipeline_->GetAudioPipeline()->SetClient(client);
  media_pipeline_->InitializeAudio(
      config, scoped_ptr<CodedFrameProvider>(), status_cb);
}

void MediaPipelineHost::VideoInitialize(
    TrackId track_id,
    const VideoPipelineClient& client,
    const ::media::VideoDecoderConfig& config,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kVideoTrackId);
  media_pipeline_->GetVideoPipeline()->SetClient(client);
  media_pipeline_->InitializeVideo(
      config, scoped_ptr<CodedFrameProvider>(), status_cb);
}

void MediaPipelineHost::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->StartPlayingFrom(time);
}

void MediaPipelineHost::Flush(const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->Flush(status_cb);
}

void MediaPipelineHost::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->Stop();
}

void MediaPipelineHost::SetPlaybackRate(float playback_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->SetPlaybackRate(playback_rate);
}

void MediaPipelineHost::SetVolume(TrackId track_id, float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(track_id == kAudioTrackId);
  media_pipeline_->GetAudioPipeline()->SetVolume(volume);
}

void MediaPipelineHost::SetCdm(::media::BrowserCdm* cdm) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_pipeline_->SetCdm(cdm);
}

void MediaPipelineHost::NotifyPipeWrite(TrackId track_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MediaTrackMap::iterator it = media_track_map_.find(track_id);
  if (it == media_track_map_.end())
    return;

  MediaTrackHost* media_track_host = it->second;
  if (!media_track_host->pipe_write_cb.is_null())
    media_track_host->pipe_write_cb.Run();
}

}  // namespace media
}  // namespace chromecast

