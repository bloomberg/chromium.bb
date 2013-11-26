// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_TV_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_TV_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "content/common/content_export.h"
#include "media/base/demuxer.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "ui/gfx/size.h"

namespace webrtc {
class VideoDecoder;
}

namespace content {

class MediaStreamDependencyFactory;
class RTCDemuxerStream;
class RTCVideoDecoderBridgeTv;

// A factory object generating |RTCVideoDecoderBridgeTv| objects. This object
// also functions as a |media::Demuxer| object to receive encoded streams from
// the |RTCVideoDecoderBridgeTv| object (which inherits from
// |webrtc::VideoDecoder|).
class CONTENT_EXPORT RTCVideoDecoderFactoryTv
    : NON_EXPORTED_BASE(public cricket::WebRtcVideoDecoderFactory),
      public media::Demuxer {
 public:
  RTCVideoDecoderFactoryTv();
  virtual ~RTCVideoDecoderFactoryTv();

  // cricket::WebRtcVideoDecoderFactory implementation.
  virtual webrtc::VideoDecoder* CreateVideoDecoder(
      webrtc::VideoCodecType type) OVERRIDE;
  virtual void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) OVERRIDE;

  // Acquires and releases the demuxer functionality of this object. Only one
  // client object can access demuxer functionality at a time. No calls to
  // |media::Demuxer| implementations should be made without acquiring it first.
  bool AcquireDemuxer();
  void ReleaseDemuxer();

  // media::Demuxer implementation.
  virtual void Initialize(media::DemuxerHost* host,
                          const media::PipelineStatusCB& cb,
                          bool enable_text_tracks) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const media::PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual media::DemuxerStream* GetStream(
      media::DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;

  // For RTCVideoDecoderBridgeTv to talk to RTCDemuxerStream.
  void InitializeStream(const gfx::Size& size);
  void QueueBuffer(scoped_refptr<media::DecoderBuffer> buffer,
                   const gfx::Size& size);

 private:
  // All private variables are lock protected.
  base::Lock lock_;
  scoped_ptr<RTCVideoDecoderBridgeTv> decoder_;

  media::PipelineStatusCB init_cb_;
  scoped_ptr<RTCDemuxerStream> stream_;

  bool is_acquired_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderFactoryTv);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_TV_H_
