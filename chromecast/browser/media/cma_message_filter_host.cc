// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cma_message_filter_host.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "chromecast/browser/media/media_pipeline_host.h"
#include "chromecast/common/media/cma_messages.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/bind_to_current_loop.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

#define FORWARD_CALL(arg_pipeline, arg_fn, ...) \
  task_runner_->PostTask( \
      FROM_HERE, \
      base::Bind(&MediaPipelineHost::arg_fn, \
                 base::Unretained(arg_pipeline), __VA_ARGS__))

namespace {

const size_t kMaxSharedMem = 8 * 1024 * 1024;

typedef std::map<uint64_t, MediaPipelineHost*> MediaPipelineCmaMap;

// Map of MediaPipelineHost instances that is accessed only from the CMA thread.
// The existence of a MediaPipelineHost* in this map implies that the instance
// is still valid.
base::LazyInstance<MediaPipelineCmaMap> g_pipeline_map_cma =
    LAZY_INSTANCE_INITIALIZER;

uint64_t GetPipelineCmaId(int process_id, int media_id) {
  return (static_cast<uint64>(process_id) << 32) +
      static_cast<uint64>(media_id);
}

MediaPipelineHost* GetMediaPipeline(int process_id, int media_id) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineCmaMap::iterator it =
      g_pipeline_map_cma.Get().find(GetPipelineCmaId(process_id, media_id));
  if (it == g_pipeline_map_cma.Get().end())
    return nullptr;
  return it->second;
}

void SetMediaPipeline(int process_id, int media_id, MediaPipelineHost* host) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  std::pair<MediaPipelineCmaMap::iterator, bool> ret =
      g_pipeline_map_cma.Get().insert(
          std::make_pair(GetPipelineCmaId(process_id, media_id), host));

  // Check there is no other entry with the same ID.
  DCHECK(ret.second != false);
}

void DestroyMediaPipeline(int process_id,
                          int media_id,
                          scoped_ptr<MediaPipelineHost> media_pipeline) {
  DCHECK(MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
  MediaPipelineCmaMap::iterator it =
      g_pipeline_map_cma.Get().find(GetPipelineCmaId(process_id, media_id));
  if (it != g_pipeline_map_cma.Get().end())
    g_pipeline_map_cma.Get().erase(it);
}

void SetCdmOnCmaThread(int render_process_id, int media_id,
                       BrowserCdmCast* cdm) {
  MediaPipelineHost* pipeline = GetMediaPipeline(render_process_id, media_id);
  if (!pipeline) {
    LOG(WARNING) << "MediaPipelineHost not alive: " << render_process_id << ","
                 << media_id;
    return;
  }

  pipeline->SetCdm(cdm);
}

// BrowserCdm instance must be retrieved/accessed on the UI thread, then
// passed to MediaPipelineHost on CMA thread.
void SetCdmOnUiThread(
    int render_process_id,
    int render_frame_id,
    int media_id,
    int cdm_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    LOG(ERROR) << "RenderProcessHost not alive for ID: " << render_process_id;
    return;
  }

  ::media::BrowserCdm* cdm = host->GetBrowserCdm(render_frame_id, cdm_id);
  if (!cdm) {
    LOG(WARNING) << "Could not find BrowserCdm (" << render_frame_id << ","
                 << cdm_id << ")";
    return;
  }

  BrowserCdmCast* browser_cdm_cast =
      static_cast<BrowserCdmCastUi*>(cdm)->browser_cdm_cast();
  MediaMessageLoop::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&SetCdmOnCmaThread,
                 render_process_id,
                 media_id,
                 browser_cdm_cast));
}

// TODO(halliwell): remove this and the NotifyExternalSurface path once
// VIDEO_HOLE is no longer required.
void EstimateVideoPlaneRect(const gfx::QuadF& quad,
                            VideoPlane::Transform* transform,
                            RectF* rect) {
  const gfx::PointF& p0 = quad.p1();
  const gfx::PointF& p1 = quad.p2();
  const gfx::PointF& p2 = quad.p3();
  const gfx::PointF& p3 = quad.p4();

  float det0 = p0.x() * p1.y() - p1.x() * p0.y();
  float det1 = p1.x() * p2.y() - p2.x() * p1.y();
  float det2 = p2.x() * p3.y() - p3.x() * p2.y();
  float det3 = p3.x() * p0.y() - p0.x() * p3.y();

  // Calculate the area of the quad (i.e. moment 0 of the polygon).
  // Note: the area can here be either positive or negative
  // depending on whether the quad is clock wise or counter clock wise.
  float area = 0.5 * (det0 + det1 + det2 + det3);
  if (area == 0.0) {
    // Empty rectangle in that case.
    *transform = VideoPlane::TRANSFORM_NONE;
    *rect = RectF(p0.x(), p0.y(), 0.0, 0.0);
    return;
  }

  // Calculate the center of gravity of the polygon
  // (i.e. moment 1 of the polygon).
  float cx = (1.0 / (6.0 * area)) *
      ((p0.x() + p1.x()) * det0 +
       (p1.x() + p2.x()) * det1 +
       (p2.x() + p3.x()) * det2 +
       (p3.x() + p0.x()) * det3);
  float cy = (1.0 / (6.0 * area)) *
      ((p0.y() + p1.y()) * det0 +
       (p1.y() + p2.y()) * det1 +
       (p2.y() + p3.y()) * det2 +
       (p3.y() + p0.y()) * det3);

  // For numerical stability, subtract the center of gravity now instead of
  // later doing:
  // mxx -= cx * cx;
  // myy -= cy * cy;
  // mxy -= cx * cy;
  // to get the centered 2nd moments.
  gfx::PointF p0c(p0.x() - cx, p0.y() - cy);
  gfx::PointF p1c(p1.x() - cx, p1.y() - cy);
  gfx::PointF p2c(p2.x() - cx, p2.y() - cy);
  gfx::PointF p3c(p3.x() - cx, p3.y() - cy);

  // Calculate the second moments of the polygon.
  float det0c = p0c.x() * p1c.y() - p1c.x() * p0c.y();
  float det1c = p1c.x() * p2c.y() - p2c.x() * p1c.y();
  float det2c = p2c.x() * p3c.y() - p3c.x() * p2c.y();
  float det3c = p3c.x() * p0c.y() - p0c.x() * p3c.y();
  float mxx = (1.0 / (12.0 * area)) *
      ((p0c.x() * p0c.x() + p0c.x() * p1c.x() + p1c.x() * p1c.x()) * det0c +
       (p1c.x() * p1c.x() + p1c.x() * p2c.x() + p2c.x() * p2c.x()) * det1c +
       (p2c.x() * p2c.x() + p2c.x() * p3c.x() + p3c.x() * p3c.x()) * det2c +
       (p3c.x() * p3c.x() + p3c.x() * p0c.x() + p0c.x() * p0c.x()) * det3c);
  float myy = (1.0 / (12.0 * area)) *
      ((p0c.y() * p0c.y() + p0c.y() * p1c.y() + p1c.y() * p1c.y()) * det0c +
       (p1c.y() * p1c.y() + p1c.y() * p2c.y() + p2c.y() * p2c.y()) * det1c +
       (p2c.y() * p2c.y() + p2c.y() * p3c.y() + p3c.y() * p3c.y()) * det2c +
       (p3c.y() * p3c.y() + p3c.y() * p0c.y() + p0c.y() * p0c.y()) * det3c);

  // Remark: the 2nd moments of a rotated & centered rectangle are given by:
  // mxx = (1/12) * (h^2 * s^2 + w^2 * c^2)
  // myy = (1/12) * (h^2 * c^2 + w^2 * s^2)
  // mxy = (1/12) * (w^2 - h^2) * c * s
  // where w = width of the original rectangle,
  //       h = height of the original rectangle,
  //       c = cos(teta) with teta the angle of the rotation,
  //       s = sin(teta)
  //
  // For reference, mxy can be calculated from the quad using:
  // float mxy = (1.0 / (24.0 * area)) *
  //    ((2.0 * p0c.x() * p0c.y() + p0c.x() * p1c.y() + p1c.x() * p0c.y() +
  //      2.0 * p1c.x() * p1c.y()) * det0c +
  //     (2.0 * p1c.x() * p1c.y() + p1c.x() * p2c.y() + p2c.x() * p1c.y() +
  //      2.0 * p2c.x() * p2c.y()) * det1c +
  //     (2.0 * p2c.x() * p2c.y() + p2c.x() * p3c.y() + p3c.x() * p2c.y() +
  //      2.0 * p3c.x() * p3c.y()) * det2c +
  //     (2.0 * p3c.x() * p3c.y() + p3c.x() * p0c.y() + p0c.x() * p3c.y() +
  //      2.0 * p0c.x() * p0c.y()) * det3c);

  // The rotation is assumed to be 0, 90, 180 or 270 degrees, so mxy = 0.0
  // Using this assumption: mxx = (1/12) h^2 or (1/12) w^2 depending on the
  // rotation. Same for myy.
  if (mxx < 0 || myy < 0) {
    // mxx and myy should be positive, only numerical errors can lead to a
    // negative value.
    LOG(WARNING) << "Numerical errors: " << mxx << " " << myy;
    *transform = VideoPlane::TRANSFORM_NONE;
    *rect = RectF(p0.x(), p0.y(), 0.0, 0.0);
    return;
  }
  float size_x = sqrt(12.0 * mxx);
  float size_y = sqrt(12.0 * myy);

  // Estimate the parameters of the rotation since teta can only be known
  // modulo 90 degrees. In previous equations, you can always swap w and h
  // and subtract 90 degrees to teta to get the exact same second moment.
  int idx_best = -1;
  float err_best = 0.0;

  // First, estimate the rotation angle assuming
  // dist(p0,p1) is equal to |size_x|,
  // dist(p0,p3) is equal to |size_y|.
  gfx::PointF r1(size_x, 0);
  gfx::PointF r2(size_x, size_y);
  gfx::PointF r3(0, size_y);
  for (int k = 0; k < 4; k++) {
    float cur_err =
        fabs((p1.x() - p0.x()) - r1.x()) + fabs((p1.y() - p0.y()) - r1.y()) +
        fabs((p2.x() - p0.x()) - r2.x()) + fabs((p2.y() - p0.y()) - r2.y()) +
        fabs((p3.x() - p0.x()) - r3.x()) + fabs((p3.y() - p0.y()) - r3.y());
    if (idx_best < 0 || cur_err < err_best) {
      idx_best = k;
      err_best = cur_err;
    }
    // 90 degree rotation.
    r1 = gfx::PointF(-r1.y(), r1.x());
    r2 = gfx::PointF(-r2.y(), r2.x());
    r3 = gfx::PointF(-r3.y(), r3.x());
  }

  // Then, estimate the rotation angle assuming:
  // dist(p0,p1) is equal to |size_y|,
  // dist(p0,p3) is equal to |size_x|.
  r1 = gfx::PointF(size_y, 0);
  r2 = gfx::PointF(size_y, size_x);
  r3 = gfx::PointF(0, size_x);
  for (int k = 0; k < 4; k++) {
    float cur_err =
        fabs((p1.x() - p0.x()) - r1.x()) + fabs((p1.y() - p0.y()) - r1.y()) +
        fabs((p2.x() - p0.x()) - r2.x()) + fabs((p2.y() - p0.y()) - r2.y()) +
        fabs((p3.x() - p0.x()) - r3.x()) + fabs((p3.y() - p0.y()) - r3.y());
    if (idx_best < 0 || cur_err < err_best) {
      idx_best = k;
      err_best = cur_err;
    }
    // 90 degree rotation.
    r1 = gfx::PointF(-r1.y(), r1.x());
    r2 = gfx::PointF(-r2.y(), r2.x());
    r3 = gfx::PointF(-r3.y(), r3.x());
  }

  *transform = static_cast<VideoPlane::Transform>(idx_best);
  *rect = RectF(cx - size_x / 2.0, cy - size_y / 2.0, size_x, size_y);
}

void UpdateVideoSurfaceHost(int surface_id, const gfx::QuadF& quad) {
  // Currently supports only one video plane.
  CHECK_EQ(surface_id, 0);

  // Convert quad into rect + transform
  VideoPlane::Transform transform = VideoPlane::TRANSFORM_NONE;
  RectF rect(0, 0, 0, 0);
  EstimateVideoPlaneRect(quad, &transform, &rect);

  VideoPlane* video_plane = CastMediaShlib::GetVideoPlane();
  video_plane->SetGeometry(
      rect,
      VideoPlane::COORDINATE_TYPE_GRAPHICS_PLANE,
      transform);
}

}  // namespace

CmaMessageFilterHost::CmaMessageFilterHost(
    int render_process_id,
    const CreateDeviceComponentsCB& create_device_components_cb)
    : content::BrowserMessageFilter(CastMediaMsgStart),
      process_id_(render_process_id),
      create_device_components_cb_(create_device_components_cb),
      task_runner_(MediaMessageLoop::GetTaskRunner()),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

CmaMessageFilterHost::~CmaMessageFilterHost() {
  DCHECK(media_pipelines_.empty());
}

void CmaMessageFilterHost::OnChannelClosing() {
  content::BrowserMessageFilter::OnChannelClosing();
  DeleteEntries();
}

void CmaMessageFilterHost::OnDestruct() const {
  content::BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool CmaMessageFilterHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CmaMessageFilterHost, message)
    IPC_MESSAGE_HANDLER(CmaHostMsg_CreateMedia, CreateMedia)
    IPC_MESSAGE_HANDLER(CmaHostMsg_DestroyMedia, DestroyMedia)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetCdm, SetCdm)
    IPC_MESSAGE_HANDLER(CmaHostMsg_CreateAvPipe, CreateAvPipe)
    IPC_MESSAGE_HANDLER(CmaHostMsg_AudioInitialize, AudioInitialize)
    IPC_MESSAGE_HANDLER(CmaHostMsg_VideoInitialize, VideoInitialize)
    IPC_MESSAGE_HANDLER(CmaHostMsg_StartPlayingFrom, StartPlayingFrom)
    IPC_MESSAGE_HANDLER(CmaHostMsg_Flush, Flush)
    IPC_MESSAGE_HANDLER(CmaHostMsg_Stop, Stop)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetPlaybackRate, SetPlaybackRate)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetVolume, SetVolume)
    IPC_MESSAGE_HANDLER(CmaHostMsg_NotifyPipeWrite, NotifyPipeWrite)
    IPC_MESSAGE_HANDLER(CmaHostMsg_NotifyExternalSurface,
                        NotifyExternalSurface)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CmaMessageFilterHost::DeleteEntries() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  for (MediaPipelineMap::iterator it = media_pipelines_.begin();
       it != media_pipelines_.end(); ) {
    scoped_ptr<MediaPipelineHost> media_pipeline(it->second);
    media_pipelines_.erase(it++);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DestroyMediaPipeline, process_id_, it->first,
                   base::Passed(&media_pipeline)));
  }
}

MediaPipelineHost* CmaMessageFilterHost::LookupById(int media_id) {
  MediaPipelineMap::iterator it = media_pipelines_.find(media_id);
  if (it == media_pipelines_.end())
    return NULL;
  return it->second;
}


// *** Handle incoming messages ***

void CmaMessageFilterHost::CreateMedia(int media_id, LoadType load_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  scoped_ptr<MediaPipelineHost> media_pipeline_host(new MediaPipelineHost());
  MediaPipelineClient client;
  client.time_update_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnTimeUpdate, weak_this_, media_id));
  client.buffering_state_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnBufferingNotification, weak_this_, media_id));
  client.error_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPlaybackError,
                 weak_this_, media_id, media::kNoTrackId));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SetMediaPipeline,
                 process_id_, media_id, media_pipeline_host.get()));
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaPipelineHost::Initialize,
                            base::Unretained(media_pipeline_host.get()),
                            load_type, client, create_device_components_cb_));
  std::pair<MediaPipelineMap::iterator, bool> ret =
    media_pipelines_.insert(
        std::make_pair(media_id, media_pipeline_host.release()));

  // Check there is no other entry with the same ID.
  DCHECK(ret.second != false);
}

void CmaMessageFilterHost::DestroyMedia(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MediaPipelineMap::iterator it = media_pipelines_.find(media_id);
  if (it == media_pipelines_.end())
    return;

  scoped_ptr<MediaPipelineHost> media_pipeline(it->second);
  media_pipelines_.erase(it);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DestroyMediaPipeline, process_id_, media_id,
                 base::Passed(&media_pipeline)));
}

void CmaMessageFilterHost::SetCdm(int media_id,
                                  int render_frame_id,
                                  int cdm_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SetCdmOnUiThread,
                 process_id_, render_frame_id, media_id, cdm_id));
}


void CmaMessageFilterHost::CreateAvPipe(
    int media_id, TrackId track_id, size_t shared_mem_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::FileDescriptor foreign_socket_handle;
  base::SharedMemoryHandle foreign_memory_handle;

  // A few sanity checks before allocating resources.
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline || !PeerHandle() || shared_mem_size > kMaxSharedMem) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Create the local/foreign sockets to signal media message
  // consune/feed events.
  // Use CancelableSyncSocket so that write is always non-blocking.
  scoped_ptr<base::CancelableSyncSocket> local_socket(
      new base::CancelableSyncSocket());
  scoped_ptr<base::CancelableSyncSocket> foreign_socket(
      new base::CancelableSyncSocket());
  if (!base::CancelableSyncSocket::CreatePair(local_socket.get(),
                                              foreign_socket.get()) ||
      foreign_socket->handle() == -1) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Shared memory used to convey media messages.
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(shared_mem_size) ||
      !shared_memory->ShareToProcess(PeerHandle(), &foreign_memory_handle)) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Note: the IPC message can be sent only once the pipe has been fully
  // configured. Part of this configuration is done in
  // |MediaPipelineHost::SetAvPipe|.
  // TODO(erickung): investigate possible memory leak here.
  // If the weak pointer in |av_pipe_set_cb| gets invalidated,
  // then |foreign_memory_handle| leaks.
  base::Closure pipe_read_activity_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPipeReadActivity, weak_this_,
                 media_id, track_id));
  base::Closure av_pipe_set_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnAvPipeSet, weak_this_,
                 media_id, track_id,
                 foreign_memory_handle, base::Passed(&foreign_socket)));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::SetAvPipe,
                 base::Unretained(media_pipeline),
                 track_id,
                 base::Passed(&shared_memory),
                 pipe_read_activity_cb,
                 av_pipe_set_cb));
}

void CmaMessageFilterHost::OnAvPipeSet(
    int media_id,
    TrackId track_id,
    base::SharedMemoryHandle foreign_memory_handle,
    scoped_ptr<base::CancelableSyncSocket> foreign_socket) {
  base::FileDescriptor foreign_socket_handle;
  foreign_socket_handle.fd = foreign_socket->handle();
  foreign_socket_handle.auto_close = false;

  // This message can only be set once the pipe has fully been configured
  // by |MediaPipelineHost|.
  Send(new CmaMsg_AvPipeCreated(
      media_id, track_id, true, foreign_memory_handle, foreign_socket_handle));
}

void CmaMessageFilterHost::AudioInitialize(
    int media_id, TrackId track_id, const ::media::AudioDecoderConfig& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_TrackStateChanged(
        media_id, track_id, ::media::PIPELINE_ERROR_ABORT));
    return;
  }

  AvPipelineClient client;
  client.eos_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnEos, weak_this_, media_id, track_id));
  client.playback_error_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnPlaybackError, weak_this_, media_id, track_id));
  client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnStatisticsUpdated, weak_this_,
                 media_id, track_id));

  ::media::PipelineStatusCB pipeline_status_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnTrackStateChanged, weak_this_,
                 media_id, track_id));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::AudioInitialize,
                 base::Unretained(media_pipeline),
                 track_id, client, config, pipeline_status_cb));
}

void CmaMessageFilterHost::VideoInitialize(
    int media_id, TrackId track_id,
    const std::vector<::media::VideoDecoderConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_TrackStateChanged(
        media_id, track_id, ::media::PIPELINE_ERROR_ABORT));
    return;
  }

  VideoPipelineClient client;
  client.av_pipeline_client.eos_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnEos, weak_this_,
                 media_id, track_id));
  client.av_pipeline_client.playback_error_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPlaybackError, weak_this_,
                 media_id, track_id));
  client.av_pipeline_client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnStatisticsUpdated, weak_this_,
                 media_id, track_id));
  client.natural_size_changed_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnNaturalSizeChanged, weak_this_,
                 media_id, track_id));

  ::media::PipelineStatusCB pipeline_status_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnTrackStateChanged, weak_this_,
                 media_id, track_id));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::VideoInitialize,
                 base::Unretained(media_pipeline),
                 track_id, client, configs, pipeline_status_cb));
}

void CmaMessageFilterHost::StartPlayingFrom(
    int media_id, base::TimeDelta time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, StartPlayingFrom, time);
}

void CmaMessageFilterHost::Flush(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_MediaStateChanged(
        media_id, ::media::PIPELINE_ERROR_ABORT));
    return;
  }
  ::media::PipelineStatusCB pipeline_status_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnMediaStateChanged, weak_this_,
                 media_id));
  FORWARD_CALL(media_pipeline, Flush, pipeline_status_cb);
}

void CmaMessageFilterHost::Stop(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::Stop,
                 base::Unretained(media_pipeline)));
}

void CmaMessageFilterHost::SetPlaybackRate(
    int media_id, double playback_rate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, SetPlaybackRate, playback_rate);
}

void CmaMessageFilterHost::SetVolume(
    int media_id, TrackId track_id, float volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, SetVolume, track_id, volume);
}

void CmaMessageFilterHost::NotifyPipeWrite(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, NotifyPipeWrite, track_id);
}

void CmaMessageFilterHost::NotifyExternalSurface(
    int surface_id,
    const gfx::PointF& p0, const gfx::PointF& p1,
    const gfx::PointF& p2, const gfx::PointF& p3) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UpdateVideoSurfaceHost, surface_id,
                 gfx::QuadF(p0, p1, p2, p3)));
}

// *** Browser to renderer messages ***

void CmaMessageFilterHost::OnMediaStateChanged(
    int media_id, ::media::PipelineStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_MediaStateChanged(media_id, status));
}

void CmaMessageFilterHost::OnTrackStateChanged(
    int media_id, TrackId track_id, ::media::PipelineStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_TrackStateChanged(media_id, track_id, status));
}

void CmaMessageFilterHost::OnPipeReadActivity(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_NotifyPipeRead(media_id, track_id));
}

void CmaMessageFilterHost::OnTimeUpdate(
    int media_id,
    base::TimeDelta media_time,
    base::TimeDelta max_media_time,
    base::TimeTicks stc) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_TimeUpdate(media_id,
                             media_time, max_media_time, stc));
}

void CmaMessageFilterHost::OnBufferingNotification(
    int media_id, ::media::BufferingState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_BufferingNotification(media_id, state));
}

void CmaMessageFilterHost::OnEos(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_Eos(media_id, track_id));
}

void CmaMessageFilterHost::OnPlaybackError(
    int media_id, TrackId track_id, ::media::PipelineStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_PlaybackError(media_id, track_id, status));
}

void CmaMessageFilterHost::OnStatisticsUpdated(
    int media_id, TrackId track_id, const ::media::PipelineStatistics& stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_PlaybackStatistics(media_id, track_id, stats));
}

void CmaMessageFilterHost::OnNaturalSizeChanged(
    int media_id, TrackId track_id, const gfx::Size& size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_NaturalSizeChanged(media_id, track_id, size));
}

}  // namespace media
}  // namespace chromecast
