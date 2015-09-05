// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_sink.h"
#include "media/base/video_capturer_source.h"

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
  //
  // Calls to these methods must be done on the main render thread.
  // Note that |callback| for frame delivery happens on the IO thread.
  //
  // Calling RemoveFromVideoTrack also not stop frame delivery through the
  // callback immediately because it may happen on another thread.
  // The added callback will be reset on the render thread.
  static void AddToVideoTrack(MediaStreamVideoSink* sink,
                              const VideoCaptureDeliverFrameCB& callback,
                              const blink::WebMediaStreamTrack& track);
  static void RemoveFromVideoTrack(MediaStreamVideoSink* sink,
                                   const blink::WebMediaStreamTrack& track);

 protected:
  ~MediaStreamVideoSink() override {}
};


}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_VIDEO_SINK_H_
