// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_H_
#define CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "media/video/encoded_video_source.h"
#include "media/video/video_encode_types.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvie.h"

namespace content {

// Class to represent an encoding capable video capture interface for the
// WebRTC component. This class expects to be registered as an encoder with
// an internal source to the WebRTC stack and will not be able to function as
// an encoder for uncompressed video frames.
class RtcEncodingVideoCapturer : public webrtc::VideoEncoder {
 public:
  RtcEncodingVideoCapturer(media::EncodedVideoSource* encoded_video_source,
                           webrtc::VideoCodecType codec_type);
  virtual ~RtcEncodingVideoCapturer();

  // webrtc::VideoEncoder implementation.
  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                             int32_t numberOfCores,
                             uint32_t maxPayloadSize) OVERRIDE;
  virtual int32_t Encode(
      const webrtc::I420VideoFrame& /* inputImage */,
      const webrtc::CodecSpecificInfo* codecSpecificInfo,
      const std::vector<webrtc::VideoFrameType>* frame_types) OVERRIDE;
  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) OVERRIDE;
  virtual int32_t Release() OVERRIDE;
  virtual int32_t SetChannelParameters(uint32_t /* packetLoss */,
                                       int rtt_in_ms) OVERRIDE;
  virtual int32_t SetRates(uint32_t newBitRate,
                           uint32_t frameRate) OVERRIDE;
 private:
  // Forward declaration for private implementation to represent the
  // encoded video source client;
  class EncodedVideoSourceClient;
  scoped_ptr<EncodedVideoSourceClient> encoded_video_source_client_;

  // Pointer to the underlying EncodedVideoSource object. It is guaranteed to
  // outlive the RtcEncodingVideoCapturer.
  media::EncodedVideoSource* encoded_video_source_;
  webrtc::VideoCodecType rtc_codec_type_;

  DISALLOW_COPY_AND_ASSIGN(RtcEncodingVideoCapturer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_ENCODING_VIDEO_CAPTURER_H_
