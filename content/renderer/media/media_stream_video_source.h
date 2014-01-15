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

class MediaStreamDependencyFactory;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// All methods calls will be done from the main render thread.
class CONTENT_EXPORT MediaStreamVideoSource
    : public MediaStreamSourceExtraData {
 public:
  explicit MediaStreamVideoSource(
      MediaStreamDependencyFactory* factory);

  // Puts |track| in the registered tracks list. Will later
  // deliver frames to it according to |constraints|.
  virtual void AddTrack(const blink::WebMediaStreamTrack& track,
                        const blink::WebMediaConstraints& constraints);

  // Removes |track| from the registered tracks list, i.e. will stop delivering
  // frame to |track|.
  virtual void RemoveTrack(const blink::WebMediaStreamTrack& track);

  // TODO(ronghuawu): Remove webrtc::VideoSourceInterface from the public
  // interface of this class.
  webrtc::VideoSourceInterface* GetAdapter() {
    return adapter_;
  }

 protected:
  // Sets an external adapter. Must be called before Init, which creates a
  // default adapter if the adapter is not set already.
  void SetAdapter(webrtc::VideoSourceInterface* adapter) {
    DCHECK(!adapter_);
    adapter_ = adapter;
  }

  // Called by the derived class before any other methods except SetAdapter.
  void Init();

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Delivers |frame| to registered tracks according to their constraints.
  // Note: current implementation assumes |frame| be contiguous layout of image
  // planes and I420.
  virtual void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  virtual ~MediaStreamVideoSource();

 private:
  MediaStreamDependencyFactory* factory_;
  scoped_refptr<webrtc::VideoSourceInterface> adapter_;
  int width_;
  int height_;
  base::TimeDelta first_frame_timestamp_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
