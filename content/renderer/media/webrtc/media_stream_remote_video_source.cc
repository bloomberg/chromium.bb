// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/native_handle_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"

namespace content {

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public base::RefCountedThreadSafe<RemoteVideoSourceDelegate>,
      public webrtc::VideoRendererInterface {
 public:
  RemoteVideoSourceDelegate(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

 protected:
  friend class base::RefCountedThreadSafe<RemoteVideoSourceDelegate>;
  ~RemoteVideoSourceDelegate() override;

  // Implements webrtc::VideoRendererInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  void SetSize(int width, int height) override;
  void RenderFrame(const cricket::VideoFrame* frame) override;

  void DoRenderFrameOnIOThread(scoped_refptr<media::VideoFrame> video_frame,
                               const media::VideoCaptureFormat& format);
 private:
  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  // |frame_pool_| is only accessed on whatever
  // thread webrtc::VideoRendererInterface::RenderFrame is called on.
  media::VideoFramePool frame_pool_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;
};

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::RemoteVideoSourceDelegate(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_message_loop_(io_message_loop),
      frame_callback_(new_frame_callback) {
}

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::~RemoteVideoSourceDelegate() {
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::SetSize(int width, int height) {
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::RenderFrame(
    const cricket::VideoFrame* frame) {
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::RenderFrame");
  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      frame->GetElapsedTime() / rtc::kNumNanosecsPerMicrosec);

  scoped_refptr<media::VideoFrame> video_frame;
  if (frame->GetNativeHandle() != NULL) {
    NativeHandleImpl* handle =
        static_cast<NativeHandleImpl*>(frame->GetNativeHandle());
    video_frame = static_cast<media::VideoFrame*>(handle->GetHandle());
    video_frame->set_timestamp(timestamp);
  } else {
    gfx::Size size(frame->GetWidth(), frame->GetHeight());
    video_frame = frame_pool_.CreateFrame(
        media::VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);

    // Non-square pixels are unsupported.
    DCHECK_EQ(frame->GetPixelWidth(), 1u);
    DCHECK_EQ(frame->GetPixelHeight(), 1u);

    int y_rows = frame->GetHeight();
    int uv_rows = frame->GetChromaHeight();
    CopyYPlane(
        frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame.get());
    CopyUPlane(
        frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame.get());
    CopyVPlane(
        frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame.get());
  }

  media::VideoPixelFormat pixel_format =
      (video_frame->format() == media::VideoFrame::YV12) ?
          media::PIXEL_FORMAT_YV12 : media::PIXEL_FORMAT_TEXTURE;

  media::VideoCaptureFormat format(
      gfx::Size(video_frame->natural_size().width(),
                video_frame->natural_size().height()),
                MediaStreamVideoSource::kUnknownFrameRate,
                pixel_format);

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread,
                 this, video_frame, format));
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::DoRenderFrameOnIOThread(
    scoped_refptr<media::VideoFrame> video_frame,
    const media::VideoCaptureFormat& format) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::DoRenderFrameOnIOThread");
  // TODO(hclam): Give the estimated capture time.
  frame_callback_.Run(video_frame, format, base::TimeTicks());
}

MediaStreamRemoteVideoSource::Observer::Observer(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::VideoTrackInterface* track)
    : main_thread_(main_thread),
#if DCHECK_IS_ON
      source_set_(false),
#endif
      track_(track),
      state_(track->state()) {
  track->RegisterObserver(this);
}

const scoped_refptr<webrtc::VideoTrackInterface>&
MediaStreamRemoteVideoSource::Observer::track() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return track_;
}

webrtc::MediaStreamTrackInterface::TrackState
MediaStreamRemoteVideoSource::Observer::state() const {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return state_;
}

void MediaStreamRemoteVideoSource::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  track_->UnregisterObserver(this);
  track_ = nullptr;
}

MediaStreamRemoteVideoSource::Observer::~Observer() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!track_.get());
}

void MediaStreamRemoteVideoSource::Observer::SetSource(
    const base::WeakPtr<MediaStreamRemoteVideoSource>& source) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!source_);
  source_ = source;
#if DCHECK_IS_ON
  // |source_set_| means that there is a MediaStreamRemoteVideoSource that
  // should handle all state changes. Since an Observer is always created when
  // a remote MediaStream changes in RemoteMediaStreamImpl::Observer::OnChanged,
  // it can happen that an Observer is created but SetSource is never called.
  source_set_ = true;
#endif
}

void MediaStreamRemoteVideoSource::Observer::OnChanged() {
  webrtc::MediaStreamTrackInterface::TrackState state = track_->state();
  main_thread_->PostTask(FROM_HERE,
      base::Bind(&MediaStreamRemoteVideoSource::Observer::OnChangedImpl,
          this, state));
}

void MediaStreamRemoteVideoSource::Observer::OnChangedImpl(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());
#if DCHECK_IS_ON
  DCHECK(source_ || !source_set_) << "Dropping a state change event. " << state;
#endif
  if (source_ && state != state_)
    source_->OnChanged(state);
  state_ = state;
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    const scoped_refptr<MediaStreamRemoteVideoSource::Observer>& observer)
    : observer_(observer), weak_factory_(this) {
  observer->SetSource(weak_factory_.GetWeakPtr());
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  DCHECK(CalledOnValidThread());
  observer_->Unregister();
  observer_ = nullptr;
}

void MediaStreamRemoteVideoSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(CalledOnValidThread());
  media::VideoCaptureFormats formats;
  // Since the remote end is free to change the resolution at any point in time
  // the supported formats are unknown.
  callback.Run(formats);
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!delegate_.get());
  delegate_ = new RemoteVideoSourceDelegate(io_message_loop(), frame_callback);
  observer_->track()->AddRenderer(delegate_.get());
  OnStartDone(MEDIA_DEVICE_OK);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() != MediaStreamVideoSource::ENDED);
  observer_->track()->RemoveRenderer(delegate_.get());
}

webrtc::VideoRendererInterface*
MediaStreamRemoteVideoSource::RenderInterfaceForTest() {
  return delegate_.get();
}

void MediaStreamRemoteVideoSource::OnChanged(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(CalledOnValidThread());
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kInitializing:
      // Ignore the kInitializing state since there is no match in
      // WebMediaStreamSource::ReadyState.
      break;
    case webrtc::MediaStreamTrackInterface::kLive:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
