// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_sink.h"

namespace media {
class VideoFrame;
}

namespace blink {
class WebMediaStreamTrack;
}

namespace content {

// MediaStreamVideoSink is an interface used for receiving video frames from a
// Video Stream Track or a Video Source.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// All methods calls will be done from the main render thread.
class CONTENT_EXPORT MediaStreamVideoSink : public MediaStreamSink {
 public:
  // An implementation of MediaStreamVideoSink should call AddToVideoTrack when
  // it is ready to receive data from a video track. Before the implementation
  // is destroyed, RemoveFromVideoTrack must be called.
  // Calls to these methods must be done on the main render thread.
  static void AddToVideoTrack(MediaStreamVideoSink* sink,
                              const blink::WebMediaStreamTrack& track);
  static void RemoveFromVideoTrack(MediaStreamVideoSink* sink,
                                   const blink::WebMediaStreamTrack& track);

  virtual void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame) = 0;

 protected:
  virtual ~MediaStreamVideoSink() {}
};


}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_
