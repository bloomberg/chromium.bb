// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_encoding_video_capturer.h"

#include "base/logging.h"
#include "media/base/encoded_bitstream_buffer.h"

namespace content {

namespace {

static const unsigned int kMaxBitrateKbps = 50 * 1000;

}  // namespace

// Client of EncodedVideoSource. This object is created and owned by the
// RtcEncodingVideoCapturer.
class RtcEncodingVideoCapturer::EncodedVideoSourceClient :
    public media::EncodedVideoSource::Client {
 public:
  EncodedVideoSourceClient(
      media::EncodedVideoSource* encoded_video_source,
      media::VideoEncodingParameters params,
      webrtc::VideoCodecType rtc_codec_type);
  virtual ~EncodedVideoSourceClient();

  // media::EncodedVideoSource::Client implementation.
  virtual void OnOpened(
      const media::VideoEncodingParameters& params) OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual void OnBufferReady(
      scoped_refptr<const media::EncodedBitstreamBuffer> buffer) OVERRIDE;
  virtual void OnConfigChanged(
      const media::RuntimeVideoEncodingParameters& params) OVERRIDE;

  // Getters and setters for bitstream properties.
  media::RuntimeVideoEncodingParameters runtime_params() const;
  void set_round_trip_time(base::TimeDelta round_trip_time);
  void set_callback(webrtc::EncodedImageCallback* callback);

  // Sets target bitrate and framerate.
  void SetRates(uint32_t target_bitrate, uint32_t frame_rate);

  // Requests key frame.
  void RequestKeyFrame();

 private:
  // Convert buffer to webrtc types and invoke encode complete callback.
  void ReportEncodedFrame(
      scoped_refptr<const media::EncodedBitstreamBuffer> buffer);

  media::VideoEncodingParameters params_;
  webrtc::VideoCodecType rtc_codec_type_;
  bool finished_;

  base::Time time_base_;
  base::TimeDelta round_trip_time_;
  media::EncodedVideoSource* encoded_video_source_;
  webrtc::EncodedImageCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(EncodedVideoSourceClient);
};

RtcEncodingVideoCapturer::EncodedVideoSourceClient::EncodedVideoSourceClient(
    media::EncodedVideoSource* encoded_video_source,
    media::VideoEncodingParameters params,
    webrtc::VideoCodecType rtc_codec_type)
    : params_(params),
      rtc_codec_type_(rtc_codec_type),
      finished_(false),
      encoded_video_source_(encoded_video_source),
      callback_(NULL) {
  DCHECK(encoded_video_source_);
  encoded_video_source_->OpenBitstream(this, params);
}

RtcEncodingVideoCapturer::EncodedVideoSourceClient::
    ~EncodedVideoSourceClient() {
  if (!finished_)
    encoded_video_source_->CloseBitstream();
}

media::RuntimeVideoEncodingParameters
    RtcEncodingVideoCapturer::EncodedVideoSourceClient::runtime_params() const {
  return params_.runtime_params;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::set_round_trip_time(
    base::TimeDelta round_trip_time) {
  round_trip_time_ = round_trip_time;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::set_callback(
    webrtc::EncodedImageCallback* callback) {
  DCHECK(!callback_);
  callback_ = callback;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::OnOpened(
    const media::VideoEncodingParameters& params) {
  params_ = params;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::OnClosed() {
  finished_ = true;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::OnBufferReady(
    scoped_refptr<const media::EncodedBitstreamBuffer> buffer) {
  DCHECK(!finished_ && buffer.get());

  // First buffer constitutes the origin of the time for this bitstream context.
  if (time_base_.is_null())
    time_base_ = buffer->metadata().timestamp;

  ReportEncodedFrame(buffer);
  encoded_video_source_->ReturnBitstreamBuffer(buffer);
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::OnConfigChanged(
    const media::RuntimeVideoEncodingParameters& params) {
  params_.runtime_params = params;
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::SetRates(
    uint32_t target_bitrate, uint32_t frame_rate) {
  params_.runtime_params.target_bitrate = target_bitrate;
  params_.runtime_params.frames_per_second = frame_rate;
  encoded_video_source_->TrySetBitstreamConfig(params_.runtime_params);
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::RequestKeyFrame() {
  encoded_video_source_->RequestKeyFrame();
}

void RtcEncodingVideoCapturer::EncodedVideoSourceClient::ReportEncodedFrame(
    scoped_refptr<const media::EncodedBitstreamBuffer> buffer) {
  if (!callback_)
    return;

  webrtc::EncodedImage image;
  webrtc::CodecSpecificInfo codecInfo;
  webrtc::RTPFragmentationHeader fragHeader;

  // TODO(hshi): remove this const_cast. Unfortunately webrtc::EncodedImage
  // defines member |_buffer| of type uint8_t* even though webrtc never modifies
  // the buffer contents.
  image._buffer = const_cast<uint8_t*>(buffer->buffer());
  image._length = buffer->size();
  image._size = image._length;

  const media::BufferEncodingMetadata& metadata = buffer->metadata();
  base::TimeDelta capture_time = metadata.timestamp - time_base_;
  image.capture_time_ms_ = capture_time.InMilliseconds();
  // Convert capture time to 90 kHz RTP timestamp.
  image._timeStamp = (capture_time * 90000).InSeconds();
  if (metadata.key_frame) {
    image._frameType = webrtc::kKeyFrame;
  } else {
    image._frameType = webrtc::kDeltaFrame;
  }
  image._completeFrame = true;
  image._encodedWidth = params_.resolution.width();
  image._encodedHeight = params_.resolution.height();

  // TODO(hshi): generate codec specific info for VP8.
  codecInfo.codecType = rtc_codec_type_;

  // Generate header containing a single fragmentation.
  fragHeader.VerifyAndAllocateFragmentationHeader(1);
  fragHeader.fragmentationOffset[0] = 0;
  fragHeader.fragmentationLength[0] = buffer->size();
  fragHeader.fragmentationPlType[0] = 0;
  fragHeader.fragmentationTimeDiff[0] = 0;

  callback_->Encoded(image, &codecInfo, &fragHeader);
}

// RtcEncodingVideoCapturer
RtcEncodingVideoCapturer::RtcEncodingVideoCapturer(
    media::EncodedVideoSource* encoded_video_source,
    webrtc::VideoCodecType codec_type)
    : encoded_video_source_(encoded_video_source),
      rtc_codec_type_(codec_type) {
}

RtcEncodingVideoCapturer::~RtcEncodingVideoCapturer() {
}

int32_t RtcEncodingVideoCapturer::InitEncode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores,
    uint32_t maxPayloadSize) {
  DCHECK(!encoded_video_source_client_);
  if (codecSettings->codecType != rtc_codec_type_)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  if (codecSettings->startBitrate > kMaxBitrateKbps ||
      codecSettings->maxBitrate > kMaxBitrateKbps)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

  // Convert |codecSettings| to |params|.
  media::VideoEncodingParameters params;
  params.codec_name = codecSettings->plName;
  params.resolution = gfx::Size(codecSettings->width, codecSettings->height);
  params.runtime_params.target_bitrate = codecSettings->startBitrate * 1000;
  params.runtime_params.max_bitrate = codecSettings->maxBitrate * 1000;
  params.runtime_params.frames_per_second = codecSettings->maxFramerate;
  encoded_video_source_client_.reset(new EncodedVideoSourceClient(
      encoded_video_source_, params, rtc_codec_type_));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RtcEncodingVideoCapturer::Encode(
    const webrtc::I420VideoFrame& /* inputImage */,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  if (frame_types && !frame_types->empty()) {
    webrtc::VideoFrameType type = frame_types->front();
    if (type == webrtc::kKeyFrame)
      encoded_video_source_client_->RequestKeyFrame();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RtcEncodingVideoCapturer::RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) {
  DCHECK(encoded_video_source_client_);
  encoded_video_source_client_->set_callback(callback);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RtcEncodingVideoCapturer::Release() {
  DCHECK(encoded_video_source_client_);
  encoded_video_source_client_.reset(NULL);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RtcEncodingVideoCapturer::SetChannelParameters(
    uint32_t /* packetLoss */,
    int rtt_in_ms) {
  if (!encoded_video_source_client_)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  encoded_video_source_client_->set_round_trip_time(
      base::TimeDelta::FromMilliseconds(rtt_in_ms));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RtcEncodingVideoCapturer::SetRates(uint32_t newBitRate,
                                           uint32_t frameRate) {
  if (!encoded_video_source_client_)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  if (newBitRate > kMaxBitrateKbps)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  encoded_video_source_client_->SetRates(newBitRate * 1000, frameRate);
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace content
