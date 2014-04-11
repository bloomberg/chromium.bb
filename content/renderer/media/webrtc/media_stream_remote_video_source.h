// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/video_frame_pool.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// MediaStreamRemoteVideoSource implements the MediaStreamVideoSource interface
// for video tracks received on a PeerConnection. The purpose of the class is
// to make sure there is no difference between a video track where the source is
// a local source and a video track where the source is a remote video track.
class CONTENT_EXPORT MediaStreamRemoteVideoSource
     : public MediaStreamVideoSource,
       NON_EXPORTED_BASE(public webrtc::VideoRendererInterface),
       NON_EXPORTED_BASE(public webrtc::ObserverInterface),
       public base::SupportsWeakPtr<MediaStreamRemoteVideoSource> {
 public:
  explicit MediaStreamRemoteVideoSource(
      webrtc::VideoTrackInterface* remote_track);
  virtual ~MediaStreamRemoteVideoSource();

 protected:
  // Implements MediaStreamVideoSource.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height) OVERRIDE;

  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params) OVERRIDE;

  virtual void StopSourceImpl() OVERRIDE;

  virtual webrtc::VideoSourceInterface* GetAdapter() OVERRIDE;

  // Implements webrtc::VideoRendererInterface used for receiving video frames
  // from the PeerConnection video track. May be called on
  // a different thread.
  virtual void SetSize(int width, int height) OVERRIDE;
  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

 private:
  void FrameFormatOnMainThread(const media::VideoCaptureFormat& format);
  void DoRenderFrameOnMainThread(scoped_refptr<media::VideoFrame> video_frame);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<webrtc::VideoTrackInterface> remote_track_;
  webrtc::MediaStreamTrackInterface::TrackState last_state_;

  // |first_frame_received_| and |frame_pool_| are only accessed on whatever
  // thread webrtc::VideoRendererInterface::RenderFrame is called on.
  bool first_frame_received_;
  media::VideoFramePool frame_pool_;

  // |format_| is the format currently received by this source.
  media::VideoCaptureFormat format_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamRemoteVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
