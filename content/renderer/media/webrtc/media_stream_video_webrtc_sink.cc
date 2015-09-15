// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

namespace {

bool ConstraintKeyExists(const blink::WebMediaConstraints& constraints,
                         const blink::WebString& name) {
  blink::WebString value_str;
  return constraints.getMandatoryConstraintValue(name, value_str) ||
      constraints.getOptionalConstraintValue(name, value_str);
}

}  // anonymouse namespace

namespace content {

// Simple help class used for receiving video frames on the IO-thread from a
// MediaStreamVideoTrack and forward the frames to a WebRtcVideoCapturerAdapter
// on libjingle's worker thread. WebRtcVideoCapturerAdapter implements a video
// capturer for libjingle.
class MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter
    : public base::RefCountedThreadSafe<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          libjingle_worker_thread,
      const scoped_refptr<webrtc::VideoSourceInterface>& source,
      WebRtcVideoCapturerAdapter* capture_adapter);

  // MediaStreamVideoWebRtcSink can be destroyed on the main render thread or
  // libjingles worker thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is to
  // make sure |video_source_| is released when MediaStreamVideoWebRtcSink() is
  // destroyed.
  void ReleaseSourceOnMainThread();

  void OnVideoFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        base::TimeTicks estimated_capture_time);

 private:
  void OnVideoFrameOnWorkerThread(
      const scoped_refptr<media::VideoFrame>& frame);
  friend class base::RefCountedThreadSafe<WebRtcVideoSourceAdapter>;
  virtual ~WebRtcVideoSourceAdapter();

  scoped_refptr<base::SingleThreadTaskRunner> render_thread_task_runner_;

  // |render_thread_checker_| is bound to the main render thread.
  base::ThreadChecker render_thread_checker_;
  // Used to DCHECK that frames are called on the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // Used for posting frames to libjingle's worker thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::SingleThreadTaskRunner> libjingle_worker_thread_;

  scoped_refptr<webrtc::VideoSourceInterface> video_source_;

  // Used to protect |capture_adapter_|. It is taken by libjingle's worker
  // thread for each video frame that is delivered but only taken on the
  // main render thread in ReleaseSourceOnMainThread() when
  // the owning MediaStreamVideoWebRtcSink is being destroyed.
  base::Lock capture_adapter_stop_lock_;
  // |capture_adapter_| is owned by |video_source_|
  WebRtcVideoCapturerAdapter* capture_adapter_;
};

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& libjingle_worker_thread,
    const scoped_refptr<webrtc::VideoSourceInterface>& source,
    WebRtcVideoCapturerAdapter* capture_adapter)
    : render_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source),
      capture_adapter_(capture_adapter) {
  io_thread_checker_.DetachFromThread();
}

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ~WebRtcVideoSourceAdapter() {
  DVLOG(3) << "~WebRtcVideoSourceAdapter()";
  DCHECK(!capture_adapter_);
  // This object can be destroyed on the main render thread or libjingles worker
  // thread since it posts video frames on that thread. But |video_source_| must
  // be released on the main render thread before the PeerConnectionFactory has
  // been destroyed. The only way to ensure that is to make sure |video_source_|
  // is released when MediaStreamVideoWebRtcSink() is destroyed.
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
ReleaseSourceOnMainThread() {
  DCHECK(render_thread_checker_.CalledOnValidThread());
  // Since frames are posted to the worker thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  // |video_source| owns |capture_adapter_|.
  capture_adapter_ = NULL;
  video_source_ = NULL;
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  libjingle_worker_thread_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                 this,
                 frame));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnVideoFrameOnWorkerThread(const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(capture_adapter_stop_lock_);
  if (capture_adapter_)
    capture_adapter_->OnFrameCaptured(frame);
}

MediaStreamVideoWebRtcSink::MediaStreamVideoWebRtcSink(
    const blink::WebMediaStreamTrack& track,
    PeerConnectionDependencyFactory* factory)
    : web_track_(track) {
  const blink::WebMediaConstraints& constraints =
      MediaStreamVideoTrack::GetVideoTrack(track)->constraints();

  bool is_screencast = ConstraintKeyExists(
      constraints, base::UTF8ToUTF16(kMediaStreamSource));
  WebRtcVideoCapturerAdapter* capture_adapter =
      factory->CreateVideoCapturer(is_screencast);

  // |video_source| owns |capture_adapter|
  scoped_refptr<webrtc::VideoSourceInterface> video_source(
      factory->CreateVideoSource(capture_adapter,
                                 track.source().constraints()));

  video_track_ = factory->CreateLocalVideoTrack(web_track_.id().utf8(),
                                                video_source.get());

  video_track_->set_enabled(web_track_.isEnabled());

  source_adapter_ = new WebRtcVideoSourceAdapter(
      factory->GetWebRtcWorkerThread(),
      video_source,
      capture_adapter);

  AddToVideoTrack(
      this,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnIO, source_adapter_),
      web_track_);

  DVLOG(3) << "MediaStreamVideoWebRtcSink ctor() : is_screencast "
           << is_screencast;
}

MediaStreamVideoWebRtcSink::~MediaStreamVideoWebRtcSink() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "MediaStreamVideoWebRtcSink dtor().";
  RemoveFromVideoTrack(this, web_track_);
  source_adapter_->ReleaseSourceOnMainThread();
}

void MediaStreamVideoWebRtcSink::OnEnabledChanged(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_track_->set_enabled(enabled);
}

}  // namespace content
