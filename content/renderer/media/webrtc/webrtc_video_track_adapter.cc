// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_track_adapter.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"

namespace {

bool ConstraintKeyExists(const blink::WebMediaConstraints& constraints,
                         const blink::WebString& name) {
  blink::WebString value_str;
  return constraints.getMandatoryConstraintValue(name, value_str) ||
      constraints.getOptionalConstraintValue(name, value_str);
}

// Used to make sure |source| is released on the main render thread.
void ReleaseWebRtcSourceOnMainRenderThread(
    webrtc::VideoSourceInterface* source) {
  source->Release();
}

}  // anonymouse namespace

namespace content {

// Simple help class used for receiving video frames on the IO-thread from
// a MediaStreamVideoTrack and forward the frames to a
// WebRtcVideoCapturerAdapter on libjingle's worker thread.
// WebRtcVideoCapturerAdapter implements a video capturer for libjingle.
class WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter
    : public base::RefCountedThreadSafe<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::MessageLoopProxy>& libjingle_worker_thread,
      const scoped_refptr<webrtc::VideoSourceInterface>& source,
      WebRtcVideoCapturerAdapter* capture_adapter);

  void OnVideoFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                        const media::VideoCaptureFormat& format);

 private:
  void OnVideoFrameOnWorkerThread(const scoped_refptr<media::VideoFrame>& frame,
                                  const media::VideoCaptureFormat& format);
  friend class base::RefCountedThreadSafe<WebRtcVideoSourceAdapter>;
  virtual ~WebRtcVideoSourceAdapter();

  scoped_refptr<base::MessageLoopProxy> render_thread_message_loop_;

  // Used to DCHECK that frames are called on the IO-thread.
  base::ThreadChecker io_thread_checker_;

  // Used for posting frames to libjingle's worker thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::MessageLoopProxy> libjingle_worker_thread_;

  scoped_refptr<webrtc::VideoSourceInterface> video_source_;
  // |capture_adapter_| is owned by |video_source_|
  WebRtcVideoCapturerAdapter* capture_adapter_;
};

WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::MessageLoopProxy>& libjingle_worker_thread,
    const scoped_refptr<webrtc::VideoSourceInterface>& source,
    WebRtcVideoCapturerAdapter* capture_adapter)
    : render_thread_message_loop_(base::MessageLoopProxy::current()),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source),
      capture_adapter_(capture_adapter) {
  io_thread_checker_.DetachFromThread();
}

WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::~WebRtcVideoSourceAdapter() {
  // Since frames are posted to the worker thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  if (!render_thread_message_loop_->BelongsToCurrentThread()) {
    webrtc::VideoSourceInterface* source = video_source_.get();
    source->AddRef();
    video_source_ = NULL;
    render_thread_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ReleaseWebRtcSourceOnMainRenderThread,
                   base::Unretained(source)));
  }
}

void WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  libjingle_worker_thread_->PostTask(
      FROM_HERE,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                 this, frame, format));
}

void
WebRtcVideoTrackAdapter::WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  capture_adapter_->OnFrameCaptured(frame);
}

WebRtcVideoTrackAdapter::WebRtcVideoTrackAdapter(
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
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnIO,
                 source_adapter_),
      web_track_);

  DVLOG(3) << "WebRtcVideoTrackAdapter ctor() : is_screencast "
           << is_screencast;
}

WebRtcVideoTrackAdapter::~WebRtcVideoTrackAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "WebRtcVideoTrackAdapter dtor().";
  RemoveFromVideoTrack(this, web_track_);
}

void WebRtcVideoTrackAdapter::OnEnabledChanged(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_track_->set_enabled(enabled);
}

}  // namespace content
