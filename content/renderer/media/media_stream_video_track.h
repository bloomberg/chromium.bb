// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_track.h"

namespace webrtc {
class VideoTrackInterface;
}

namespace content {

class MediaStreamDependencyFactory;
class WebRtcVideoSinkAdapter;

// MediaStreamVideoTrack is a video specific representation of a
// blink::WebMediaStreamTrack in content. It is owned by the blink object
// and can be retrieved from a blink object using
// WebMediaStreamTrack::extraData()
class CONTENT_EXPORT MediaStreamVideoTrack : public MediaStreamTrack {
 public:
  // Constructor for local video tracks.
  explicit MediaStreamVideoTrack(MediaStreamDependencyFactory* factory);
  // Constructor for remote video tracks.
  explicit MediaStreamVideoTrack(webrtc::VideoTrackInterface* track);
  virtual ~MediaStreamVideoTrack();
  void AddSink(MediaStreamVideoSink* sink);
  void RemoveSink(MediaStreamVideoSink* sink);

  virtual webrtc::VideoTrackInterface* GetVideoAdapter() OVERRIDE;

 private:
  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;
  ScopedVector<WebRtcVideoSinkAdapter> sinks_;

  // Weak ref to a MediaStreamDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  MediaStreamDependencyFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_TRACK_H_
