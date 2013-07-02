// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "media/video/encoded_video_source.h"
#include "media/video/video_encode_types.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoencoderfactory.h"

namespace content {

class RtcEncodingVideoCapturer;

class RtcEncodingVideoCapturerFactory :
      public cricket::WebRtcVideoEncoderFactory,
      public base::SupportsWeakPtr<RtcEncodingVideoCapturerFactory> {
 public:
  RtcEncodingVideoCapturerFactory();

  // WebRtcVideoEncoderFactory interface.
  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) OVERRIDE;
  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) OVERRIDE;

  virtual const std::vector<VideoCodec>& codecs() const OVERRIDE;

  virtual void AddObserver(
      WebRtcVideoEncoderFactory::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      WebRtcVideoEncoderFactory::Observer* observer) OVERRIDE;

  // Callback for RequestCapabilities().
  void OnCapabilitiesAvailable(const media::VideoEncodingCapabilities& caps);

  // Invoked when an EncodedVideoSource is added/removed.
  void OnEncodedVideoSourceAdded(media::EncodedVideoSource* source);
  void OnEncodedVideoSourceRemoved(media::EncodedVideoSource* source);

private:
  virtual ~RtcEncodingVideoCapturerFactory();

  // Set of registered encoding capable video capturers.
  typedef std::set<webrtc::VideoEncoder*> EncoderSet;

  media::EncodedVideoSource* encoded_video_source_;
  std::vector<VideoCodec> codecs_;

  EncoderSet encoder_set_;

  ObserverList<WebRtcVideoEncoderFactory::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(RtcEncodingVideoCapturerFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_FACTORY_H_
