// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/media_stream_video_source.h"

namespace webrtc {
class VideoTrackInterface;
}

namespace content {

class MediaStreamDependencyFactory;
class WebRtcVideoSinkAdapter;

// MediaStreamVideoTrack is a video specific representation of a
// blink::WebMediaStreamTrack in content. It is owned by the blink object
// and can be retrieved from a blink object using
// WebMediaStreamTrack::extraData() or MediaStreamVideoTrack::GetVideoTrack.
class CONTENT_EXPORT MediaStreamVideoTrack : public MediaStreamTrack {
 public:
  // Help method to create a blink::WebMediaStreamTrack and a
  // MediaStreamVideoTrack instance. The MediaStreamVideoTrack object is owned
  // by the blink object in its WebMediaStreamTrack::ExtraData member.
  // |callback| is triggered if the track is added to the source
  // successfully and will receive video frames that match |constraints|
  // or if the source fail to provide video frames.
  // If |enabled| is true, sinks added to the track will
  // receive video frames when the source deliver frames to the track.
  static blink::WebMediaStreamTrack CreateVideoTrack(
      MediaStreamVideoSource* source,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamVideoSource::ConstraintsCallback& callback,
      bool enabled,
      MediaStreamDependencyFactory* factory);

  static MediaStreamVideoTrack* GetVideoTrack(
      const blink::WebMediaStreamTrack& track);

  // Constructor for local video tracks.
   MediaStreamVideoTrack(
       MediaStreamVideoSource* source,
       const blink::WebMediaConstraints& constraints,
       const MediaStreamVideoSource::ConstraintsCallback& callback,
       bool enabled,
       MediaStreamDependencyFactory* factory);
  virtual ~MediaStreamVideoTrack();
  virtual void AddSink(MediaStreamVideoSink* sink);
  virtual void RemoveSink(MediaStreamVideoSink* sink);

  // TODO(perkj): GetVideoAdapter is webrtc specific. Move GetVideoAdapter to
  // where the track is added to a RTCPeerConnection. crbug/323223.
  virtual webrtc::VideoTrackInterface* GetVideoAdapter() OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame);
  void OnReadyStateChanged(blink::WebMediaStreamSource::ReadyState state);

 protected:
  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

 private:
  bool enabled_;
  std::vector<MediaStreamVideoSink*> sinks_;

  // Weak ref to the source this tracks is connected to.  |source_| is owned
  // by the blink::WebMediaStreamSource and is guaranteed to outlive the
  // track.
  MediaStreamVideoSource* source_;

  // Weak ref to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrack);
};

// WebRtcMediaStreamVideoTrack is a content representation of a video track.
// received on a PeerConnection.
// TODO(perkj): Replace WebRtcMediaStreamVideoTrack with a remote
// MediaStreamVideoSource class so that all tracks are MediaStreamVideoTracks
// and new tracks can be cloned from the original remote video track.
// crbug/334243.
class CONTENT_EXPORT WebRtcMediaStreamVideoTrack
    : public MediaStreamVideoTrack {
 public:
  explicit WebRtcMediaStreamVideoTrack(webrtc::VideoTrackInterface* track);
  virtual ~WebRtcMediaStreamVideoTrack();
  virtual void AddSink(MediaStreamVideoSink* sink) OVERRIDE;
  virtual void RemoveSink(MediaStreamVideoSink* sink) OVERRIDE;

 private:
  ScopedVector<WebRtcVideoSinkAdapter> sinks_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaStreamVideoTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
