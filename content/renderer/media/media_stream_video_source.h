// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_source_extra_data.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class VideoFrame;
}

namespace content {

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// All methods calls will be done from the main render thread.
class CONTENT_EXPORT MediaStreamVideoSource
    : public MediaStreamSourceExtraData {
 public:
  // Puts |track| in the registered tracks list. Will later
  // deliver frames to it according to |constraints|.
  void AddTrack(const blink::WebMediaStreamTrack& track,
                const blink::WebMediaConstraints& constraints);

  // Removes |track| from the registered tracks list, i.e. will stop delivering
  // frame to |track|.
  void RemoveTrack(const blink::WebMediaStreamTrack& track);

 protected:
  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Delivers |frame| to registered tracks according to their constraints.
  virtual void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  virtual ~MediaStreamVideoSource();
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
