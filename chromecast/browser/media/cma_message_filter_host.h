// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_FILTER_HOST_H_
#define CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_FILTER_HOST_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/common/media/cma_ipc_common.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"

namespace base {
class CancelableSyncSocket;
class SingleThreadTaskRunner;
}

namespace gfx {
class PointF;
class Size;
}

namespace media {
class AudioDecoderConfig;
class BrowserCdm;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {

class MediaPipelineHost;

class CmaMessageFilterHost
    : public content::BrowserMessageFilter {
 public:
  explicit CmaMessageFilterHost(int render_process_id);

  // content::BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  typedef std::map<int, MediaPipelineHost*> MediaPipelineMap;

  friend class content::BrowserThread;
  friend class base::DeleteHelper<CmaMessageFilterHost>;
  ~CmaMessageFilterHost() override;

  void DeleteEntries();
  MediaPipelineHost* LookupById(int media_id);

  // Handling of incoming IPC messages.
  void CreateMedia(int media_id, LoadType load_type);
  void DestroyMedia(int media_id);
  void SetCdm(int media_id, int render_frame_id, int cdm_id);
  void SetCdmOnUiThread(MediaPipelineHost* media_pipeline,
                        int render_frame_id,
                        int cdm_id);
  void CreateAvPipe(int media_id, TrackId track_id, size_t shared_mem_size);
  void OnAvPipeSet(int media_id,
                   TrackId track_id,
                   base::SharedMemoryHandle foreign_memory_handle,
                   scoped_ptr<base::CancelableSyncSocket> foreign_socket);
  void AudioInitialize(int media_id,
                       TrackId track_id,
                       const ::media::AudioDecoderConfig& config);
  void VideoInitialize(int media_id,
                       TrackId track_id,
                       const ::media::VideoDecoderConfig& config);
  void StartPlayingFrom(int media_id, base::TimeDelta time);
  void Flush(int media_id);
  void Stop(int media_id);
  void SetPlaybackRate(int media_id, float playback_rate);
  void SetVolume(int media_id, TrackId track_id, float volume);
  void NotifyPipeWrite(int media_id, TrackId track_id);
  void NotifyExternalSurface(int surface_id,
                             const gfx::PointF& p0, const gfx::PointF& p1,
                             const gfx::PointF& p2, const gfx::PointF& p3);

  // Audio/Video callbacks.
  void OnMediaStateChanged(int media_id,
                           ::media::PipelineStatus status);
  void OnTrackStateChanged(int media_id,
                           TrackId track_id,
                           ::media::PipelineStatus status);
  void OnPipeReadActivity(int media_id, TrackId track_id);
  void OnTimeUpdate(int media_id,
                    base::TimeDelta media_time,
                    base::TimeDelta max_media_time,
                    base::TimeTicks stc);
  void OnBufferingNotification(int media_id, ::media::BufferingState state);
  void OnEos(int media_id, TrackId track_id);
  void OnPlaybackError(int media_id, TrackId track_id,
                       ::media::PipelineStatus status);
  void OnStatisticsUpdated(int media_id,
                           TrackId track_id,
                           const ::media::PipelineStatistics& stats);
  void OnNaturalSizeChanged(int media_id,
                            TrackId track_id,
                            const gfx::Size& size);

  // Render process ID correponding to this message filter.
  const int process_id_;

  // List of media pipeline and message loop media pipelines are running on.
  MediaPipelineMap media_pipelines_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtr<CmaMessageFilterHost> weak_this_;
  base::WeakPtrFactory<CmaMessageFilterHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CmaMessageFilterHost);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_FILTER_HOST_H_

