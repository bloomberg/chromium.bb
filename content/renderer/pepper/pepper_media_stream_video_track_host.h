// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "ui/gfx/size.h"

namespace content {

class PepperMediaStreamVideoTrackHost : public PepperMediaStreamTrackHostBase,
                                        public MediaStreamVideoSink {
 public:
  PepperMediaStreamVideoTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource,
                                  const blink::WebMediaStreamTrack& track);

 private:
  virtual ~PepperMediaStreamVideoTrackHost();

  // PepperMediaStreamTrackHostBase overrides:
  virtual void OnClose() OVERRIDE;

  // MediaStreamVideoSink overrides:
  virtual void OnVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;

  // ResourceHost overrides:
  virtual void DidConnectPendingHostToResource() OVERRIDE;

  blink::WebMediaStreamTrack track_;

  // True if it has been added to |blink::WebMediaStreamTrack| as a sink.
  bool connected_;

  // Frame size.
  gfx::Size frame_size_;

  // Frame format.
  media::VideoFrame::Format frame_format_;

  // The size of frame pixels in bytes.
  uint32_t frame_data_size_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamVideoTrackHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_
